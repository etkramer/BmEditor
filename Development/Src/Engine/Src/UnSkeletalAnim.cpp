/*=============================================================================
	UnSkeletalAnim.cpp: Skeletal mesh animation functions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSequenceClasses.h"
#include "AnimationCompression.h"
#include "AnimationEncodingFormat.h"
#include "AnimationUtils.h"
#include "PerfMem.h"
#include "EngineParticleClasses.h"
#include "EngineForceFieldClasses.h"
// Priority with which to display sounds triggered by sound notifies.
#define SUBTITLE_PRIORITY_ANIMNOTIFY	10000

IMPLEMENT_CLASS(UAnimSequence)
IMPLEMENT_CLASS(UAnimSet)
IMPLEMENT_CLASS(UAnimNotify)
IMPLEMENT_CLASS(URAnimZip_Settings)

IMPLEMENT_CLASS(UAnimMetaData)
IMPLEMENT_CLASS(UAnimMetaData_SkelControl)
IMPLEMENT_CLASS(UAnimMetaData_SkelControlKeyFrame)

IMPLEMENT_CLASS(UHeadTrackingComponent)

#define USE_SLERP 0

//@deprecated with VER_REPLACED_LAZY_ARRAY_WITH_UNTYPED_BULK_DATA
struct FRawAnimSequenceTrackNativeDeprecated
{
    TArray<FVector> PosKeys;
    TArray<FQuat>	RotKeys;
	friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrackNativeDeprecated& T)
	{
		return	Ar << T.PosKeys << T.RotKeys;
	}
};

/** Returns TRUE if valid curve weight exists in the array*/
UBOOL FCurveTrack::IsValidCurveTrack()
{
	UBOOL bValid = FALSE;

	if ( CurveName != NAME_None )
	{
		for (INT I=0; I<CurveWeights.Num(); ++I)
		{
			// it has valid weight
			if (CurveWeights(I)>KINDA_SMALL_NUMBER)
			{
				bValid = TRUE;
				break;
			}
		}
	}

	return bValid;
}

/** This is very simple cut to 1 key method if all is same since I see so many redundant same value in every frame 
 *  Eventually this can get more complicated 
 *  Will return TRUE if compressed to 1. Return FALSE otherwise **/
UBOOL FCurveTrack::CompressCurveWeights()
{
	// if always 1, no reason to do this
	if ( CurveWeights.Num() > 1 )
	{
		UBOOL bCompress = TRUE;
		// first weight
		FLOAT FirstWeight = CurveWeights(0);

		for (INT I=1; I<CurveWeights.Num(); ++I)
		{
			// see if my key is same as previous
			if (fabs(FirstWeight - CurveWeights(I)) > SMALL_NUMBER)
			{
				// if not same, just get out, you don't like to compress this to 1 key
				bCompress = FALSE;
				break;
			}
		} 

		if (bCompress)
		{
			CurveWeights.Empty();
			CurveWeights.AddItem(FirstWeight);
			CurveWeights.Shrink();
		}

		return bCompress;
	}

	// nothing changed
	return FALSE;
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT UAnimSequence::GetResourceSize()
{
	if (GExclusiveResourceSizeMode)
	{
		// Sizes below are fully covered by the count serializer used when determining Num
		return 0;
	}
	else
	{
		const INT ResourceSize = CompressedTrackOffsets.Num() == 0 ? GetApproxRawSize() : GetApproxCompressedSize();
		return ResourceSize;
	}
}

/**
 * @return		The approximate size of raw animation data.
 */
INT UAnimSequence::GetApproxRawSize() const
{
	INT Total = sizeof(FRawAnimSequenceTrack) * RawAnimationData.Num();
	for (INT i=0;i<RawAnimationData.Num();++i)
	{
		const FRawAnimSequenceTrack& RawTrack = RawAnimationData(i);
		Total +=
			sizeof( FVector ) * RawTrack.PosKeys.Num() +
			sizeof( FQuat ) * RawTrack.RotKeys.Num();
	}
	return Total;
}

/**
 * @return		The approximate size of key-reduced animation data.
 */
INT UAnimSequence::GetApproxReducedSize() const
{
	return 0;
}


/**
 * @return		The approximate size of compressed animation data.
 */
INT UAnimSequence::GetApproxCompressedSize() const
{
	const INT Total = sizeof(INT)*CompressedTrackOffsets.Num() + CompressedByteStream.Num();
	return Total;
}

/**
 * Deserializes old compressed track formats from the specified archive.
 */
static void LoadOldCompressedTrack(FArchive& Ar, FCompressedTrack& Dst, INT ByteStreamStride)
{
	// Serialize from the archive to a buffer.
	INT NumBytes = 0;
	Ar << NumBytes;

	TArray<BYTE> SerializedData;
	SerializedData.Empty( NumBytes );
	SerializedData.Add( NumBytes );
	Ar.Serialize( SerializedData.GetData(), NumBytes );

	// Serialize the key times.
	Ar << Dst.Times;

	// Serialize mins and ranges.
	Ar << Dst.Mins[0] << Dst.Mins[1] << Dst.Mins[2];
	Ar << Dst.Ranges[0] << Dst.Ranges[1] << Dst.Ranges[2];
}

#if BATMAN

// AnimZip runtime decompression. Data stays compressed and is sampled directly
// via AnimZip_Sample / AnimZip_Sample_Track, matching the game's architecture.

// 24 bytes - header at start of AnimZip_Data buffer
struct FAnim
{
	INT MotionRotationBundleOffset;
	INT MotionTranslationScaleBundleOffset;
	INT NumRotationBundles;
	INT RotationBundlesOffset;
	INT NumTranslationScaleBundles;
	INT TranslationScaleBundlesOffset;
};

// 12 bytes - groups tracks sharing a codec and frame count
#pragma pack(push, 1)
struct FBundle
{
	BYTE Codec;
	BYTE NumTracks;
	USHORT NumFrames;
	INT TracksAndHeadersOffset;
	INT KeyframesOffset;
};
#pragma pack(pop)

// Resolved pointers into AnimZip_Data for a single bundle
struct FResolvedBundle
{
	const BYTE* TrackToAnimTrack;	// NumTracks bytes: maps bundle track -> anim track index
	const BYTE* Headers;			// Codec-specific per-track headers (follows TrackToAnimTrack)
	const BYTE* Keyframes;			// Keyframe data

	void Resolve(const BYTE* Data, const FBundle& B)
	{
		TrackToAnimTrack = &Data[B.TracksAndHeadersOffset];
		Headers = &Data[B.TracksAndHeadersOffset + B.NumTracks];
		Keyframes = &Data[B.KeyframesOffset];
	}
};

// Four frame indices [i-1, i, i+1, i+2] clamped to range (Default.xex.c FCatmullRomTime::Make).
struct FCatmullRomTime
{
	INT Frame0;		// i-1
	INT Frame1;		// i (segment start)
	INT Frame2;		// i+1 (segment end)
	INT Frame3;		// i+2
	FLOAT Alpha;	// fractional position within [Frame1, Frame2]

	static FCatmullRomTime Make(FLOAT NormalizedTime, INT NumFrames)
	{
		FCatmullRomTime T;
		if (NumFrames <= 1)
		{
			T.Frame0 = T.Frame1 = T.Frame2 = T.Frame3 = 0;
			T.Alpha = 0.0f;
			return T;
		}
		FLOAT Pos = NormalizedTime * (NumFrames - 1);
		INT i = Clamp<INT>(appFloor(Pos), 0, NumFrames - 1);
		T.Frame1 = i;
		T.Frame2 = Min(i + 1, NumFrames - 1);
		T.Frame0 = Max(i - 1, 0);
		T.Frame3 = Min(i + 2, NumFrames - 1);
		T.Alpha = Pos - (FLOAT)i;
		return T;
	}
};

static FQuat DecodeQuatMax48(const BYTE* b)
{
	static const float shift = 0.70710678118f;
	static const float scale = 1.41421356237f;
	float scale0 = scale / ((1 << 15) - 1);

	int mVal = ((b[0] << 8) | b[1]) & 0x7FFF;
	int hVal = ((b[2] << 8) | b[3]) & 0x7FFF;
	int lVal = ((b[4] << 8) | b[5]) & 0x7FFF;
	int S = ((b[2] >> 6) & 2) | (b[4] >> 7);

	float l = lVal * scale0 - shift;
	float m = mVal * scale0 - shift;
	float h = hVal * scale0 - shift;
	float wSq = 1.0f - (l * l + m * m + h * h);
	float a = (wSq > 0) ? appSqrt(wSq) : 0;

	switch (S)
	{
	case 0: return FQuat(a, m, h, l);
	case 1: return FQuat(m, a, h, l);
	case 2: return FQuat(m, h, a, l);
	default: return FQuat(m, h, l, a);
	}
}

static FQuat DecodeQuatMax40(const BYTE* b)
{
	static const float shift = 0.70710678118f;
	static const float scale = 1.41421356237f;
	float scale0 = scale / ((1 << 12) - 1);

	int mVal = (((b[0] << 8) | b[1]) >> 4) & 0xFFF;
	int hVal = ((b[1] << 8) | b[2]) & 0xFFF;
	int lVal = (((b[3] << 8) | b[4]) >> 4) & 0xFFF;
	int S = b[4] & 3;

	float l = lVal * scale0 - shift;
	float m = mVal * scale0 - shift;
	float h = hVal * scale0 - shift;
	float wSq = 1.0f - (l * l + m * m + h * h);
	float a = (wSq > 0) ? appSqrt(wSq) : 0;

	switch (S)
	{
	case 0: return FQuat(a, m, h, l);
	case 1: return FQuat(m, a, h, l);
	case 2: return FQuat(m, h, a, l);
	default: return FQuat(m, h, l, a);
	}
}

static FQuat DecodeQuatRelative(INT Shift, UINT X, UINT Y, UINT Z, const char* Interval)
{
	FQuat Base;
	Base.X = Interval[0] / 127.0f;
	Base.Y = Interval[1] / 127.0f;
	Base.Z = Interval[2] / 127.0f;
	Base.W = Interval[3] / 127.0f;
	Base.Normalize();

	float Scale = 1.0f / ((1 << Shift) - 1);
	FQuat Delta;
	Delta.X = X * Scale * ((BYTE)Interval[7]) / 127.5f + Interval[4] / 127.0f;
	Delta.Y = Y * Scale * ((BYTE)Interval[8]) / 127.5f + Interval[5] / 127.0f;
	Delta.Z = Z * Scale * ((BYTE)Interval[9]) / 127.5f + Interval[6] / 127.0f;
	float wSq = 1.0f - (Delta.X * Delta.X + Delta.Y * Delta.Y + Delta.Z * Delta.Z);
	Delta.W = (wSq > 0) ? appSqrt(wSq) : 0;

	Base *= Delta;
	return Base;
}

static FQuat DecodeQuatRelative32(const BYTE* KeyData, const char* Interval)
{
	UINT val32 = (KeyData[0] << 24) | (KeyData[1] << 16) | (KeyData[2] << 8) | KeyData[3];
	return DecodeQuatRelative(10, (val32 >> 20) & 0x3FF, (val32 >> 10) & 0x3FF, val32 & 0x3FF, Interval);
}

static FQuat DecodeQuatRelative24(const BYTE* KeyData, const char* Interval)
{
	return DecodeQuatRelative(8, KeyData[0], KeyData[1], KeyData[2], Interval);
}

static FQuat DecodeQuatRelative16(const BYTE* KeyData, const char* Interval)
{
	UINT val16 = (KeyData[0] << 8) | KeyData[1];
	return DecodeQuatRelative(5, (val16 >> 10) & 0x1F, (val16 >> 5) & 0x1F, val16 & 0x1F, Interval);
}

static FQuat DecodeQuatFixedAxis(INT Shift, UINT Value, const BYTE* Interval)
{
	FQuat r(0, 0, 0, 0);
	switch (Interval[0])
	{
	case 0: r.X = 1; break;
	case 1: r.Y = 1; break;
	default: r.Z = 1; break;
	}
	static const float AngleScale = PI * 2 / 255;
	float Angle = (Interval[1] * AngleScale + Value * Interval[2] * AngleScale / ((1 << Shift) - 1)) * 0.5f;
	float AngleSin = appSin(Angle);
	r.X *= AngleSin;
	r.Y *= AngleSin;
	r.Z *= AngleSin;
	r.W = appCos(Angle);
	return r;
}

static const INT GRotationKeyframeSizes[AZRC_MAX] = { 6, 5, 4, 3, 2, 2, 1 };
static const INT GRotationHeaderSizes[AZRC_MAX] = { 0, 0, 10, 10, 10, 3, 3 };
static const INT GTranslationKeyframeSizes[AZTSC_MAX] = { 16, 12, 6, 3 };
static const INT GTranslationHeaderSizes[AZTSC_MAX] = { 0, 0, 24, 24 };

static FQuat SampleRotationKey(BYTE Codec, INT TrackInBundle, INT Frame, INT NumTracks,
	const BYTE* Headers, const BYTE* Keyframes)
{
	INT KeySize = GRotationKeyframeSizes[Codec];
	const BYTE* Key = &Keyframes[KeySize * (TrackInBundle + NumTracks * Frame)];

	switch (Codec)
	{
	case AZRC_QuatMax_48:
		return DecodeQuatMax48(Key);
	case AZRC_QuatMax_40:
		return DecodeQuatMax40(Key);
	case AZRC_QuatRelative_32:
	{
		const char* Interval = (const char*)&Headers[10 * TrackInBundle];
		return DecodeQuatRelative32(Key, Interval);
	}
	case AZRC_QuatRelative_24:
	{
		const char* Interval = (const char*)&Headers[10 * TrackInBundle];
		return DecodeQuatRelative24(Key, Interval);
	}
	case AZRC_QuatRelative_16:
	{
		const char* Interval = (const char*)&Headers[10 * TrackInBundle];
		return DecodeQuatRelative16(Key, Interval);
	}
	case AZRC_FixedAxis_16:
	{
		const BYTE* Interval = &Headers[3 * TrackInBundle];
		USHORT w = *(const USHORT*)Key;  // Native LE - FixedAxis stores native USHORT
		return DecodeQuatFixedAxis(16, w, Interval);
	}
	case AZRC_FixedAxis_8:
	{
		const BYTE* Interval = &Headers[3 * TrackInBundle];
		return DecodeQuatFixedAxis(8, Key[0], Interval);
	}
	default:
		return FQuat::Identity;
	}
}

static FQuat SampleRotationBundle(const FResolvedBundle& RB, BYTE Codec, INT TrackInBundle,
	INT NumTracks, const FCatmullRomTime& Time)
{
	FQuat K1 = SampleRotationKey(Codec, TrackInBundle, Time.Frame1, NumTracks, RB.Headers, RB.Keyframes);
	if (Time.Alpha <= 0.0f || Time.Frame1 == Time.Frame2)
	{
		return K1;
	}

	FQuat K0 = SampleRotationKey(Codec, TrackInBundle, Time.Frame0, NumTracks, RB.Headers, RB.Keyframes);
	FQuat K2 = SampleRotationKey(Codec, TrackInBundle, Time.Frame2, NumTracks, RB.Headers, RB.Keyframes);
	FQuat K3 = SampleRotationKey(Codec, TrackInBundle, Time.Frame3, NumTracks, RB.Headers, RB.Keyframes);

	// Align all keys to K1's hemisphere (so the cubic blend takes the short way).
	if ((K1 | K0) < 0.0f) { K0 = FQuat(-K0.X, -K0.Y, -K0.Z, -K0.W); }
	if ((K1 | K2) < 0.0f) { K2 = FQuat(-K2.X, -K2.Y, -K2.Z, -K2.W); }
	if ((K1 | K3) < 0.0f) { K3 = FQuat(-K3.X, -K3.Y, -K3.Z, -K3.W); }

	const FLOAT a = Time.Alpha;
	const FLOAT a2 = a * a;
	const FLOAT a3 = a2 * a;
	const FLOAT b0 = -0.5f * a3 +        a2 - 0.5f * a;
	const FLOAT b1 =  1.5f * a3 - 2.5f * a2             + 1.0f;
	const FLOAT b2 = -1.5f * a3 + 2.0f * a2 + 0.5f * a;
	const FLOAT b3 =  0.5f * a3 - 0.5f * a2;

	FQuat Result(
		K0.X * b0 + K1.X * b1 + K2.X * b2 + K3.X * b3,
		K0.Y * b0 + K1.Y * b1 + K2.Y * b2 + K3.Y * b3,
		K0.Z * b0 + K1.Z * b1 + K2.Z * b2 + K3.Z * b3,
		K0.W * b0 + K1.W * b1 + K2.W * b2 + K3.W * b3);
	Result.Normalize();
	return Result;
}

static FVector SampleTranslationKey(BYTE Codec, INT TrackInBundle, INT Frame, INT NumTracks,
	const BYTE* Headers, const BYTE* Keyframes, FLOAT* OutScale = NULL)
{
	INT KeySize = GTranslationKeyframeSizes[Codec];
	const BYTE* Key = &Keyframes[KeySize * (TrackInBundle + NumTracks * Frame)];

	switch (Codec)
	{
	case AZTSC_Float_128:
	{
		const FLOAT* f = (const FLOAT*)Key;
		if (OutScale) *OutScale = f[3];
		return FVector(f[0], f[1], f[2]);
	}
	case AZTSC_NoScale_Float_96:
	{
		const FLOAT* f = (const FLOAT*)Key;
		if (OutScale) *OutScale = 1.0f;
		return FVector(f[0], f[1], f[2]);
	}
	case AZTSC_NoScale_Interval_Fixed_48:
	{
		const FLOAT* Hdr = (const FLOAT*)&Headers[24 * TrackInBundle];
		const SHORT* vi = (const SHORT*)Key;
		FVector v;
		v.X = vi[0] / 32767.0f * Hdr[3] + Hdr[0];
		v.Y = vi[1] / 32767.0f * Hdr[4] + Hdr[1];
		v.Z = vi[2] / 32767.0f * Hdr[5] + Hdr[2];
		if (OutScale) *OutScale = 1.0f;
		return v;
	}
	case AZTSC_NoScale_Interval_Fixed_24:
	{
		const FLOAT* Hdr = (const FLOAT*)&Headers[24 * TrackInBundle];
		const INT8* vi = (const INT8*)Key;
		FVector v;
		v.X = vi[0] / 127.0f * Hdr[3] + Hdr[0];
		v.Y = vi[1] / 127.0f * Hdr[4] + Hdr[1];
		v.Z = vi[2] / 127.0f * Hdr[5] + Hdr[2];
		if (OutScale) *OutScale = 1.0f;
		return v;
	}
	default:
		if (OutScale) *OutScale = 1.0f;
		return FVector::ZeroVector;
	}
}

static FVector SampleTranslationBundle(const FResolvedBundle& RB, BYTE Codec, INT TrackInBundle,
	INT NumTracks, const FCatmullRomTime& Time)
{
	FVector V1 = SampleTranslationKey(Codec, TrackInBundle, Time.Frame1, NumTracks, RB.Headers, RB.Keyframes);
	if (Time.Alpha <= 0.0f || Time.Frame1 == Time.Frame2)
	{
		return V1;
	}
	FVector V0 = SampleTranslationKey(Codec, TrackInBundle, Time.Frame0, NumTracks, RB.Headers, RB.Keyframes);
	FVector V2 = SampleTranslationKey(Codec, TrackInBundle, Time.Frame2, NumTracks, RB.Headers, RB.Keyframes);
	FVector V3 = SampleTranslationKey(Codec, TrackInBundle, Time.Frame3, NumTracks, RB.Headers, RB.Keyframes);

	const FLOAT a = Time.Alpha;
	const FLOAT a2 = a * a;
	const FLOAT a3 = a2 * a;
	const FLOAT b0 = -0.5f * a3 +        a2 - 0.5f * a;
	const FLOAT b1 =  1.5f * a3 - 2.5f * a2             + 1.0f;
	const FLOAT b2 = -1.5f * a3 + 2.0f * a2 + 0.5f * a;
	const FLOAT b3 =  0.5f * a3 - 0.5f * a2;

	return V0 * b0 + V1 * b1 + V2 * b2 + V3 * b3;
}

void AnimZip_Sample_Track(const UAnimSequence* Seq, INT TrackIndex, FLOAT NormalizedTime, FBoneAtom* Out)
{
	const BYTE* Data = Seq->AnimZip_Data.GetData();
	const FAnim* Anim = (const FAnim*)Data;

	NormalizedTime = Clamp(NormalizedTime, 0.0f, 1.0f - (FLOAT)SMALL_NUMBER);

	UBOOL bFoundRotation = FALSE;
	const FBundle* RotBundles = (const FBundle*)&Data[Anim->RotationBundlesOffset];
	for (INT i = 0; i < Anim->NumRotationBundles; i++)
	{
		const FBundle& B = RotBundles[i];
		const BYTE* TrackMap = &Data[B.TracksAndHeadersOffset];
		for (INT t = 0; t < B.NumTracks; t++)
		{
			if (TrackMap[t] == TrackIndex)
			{
				FResolvedBundle RB;
				RB.Resolve(Data, B);
				FCatmullRomTime Time = FCatmullRomTime::Make(NormalizedTime, B.NumFrames);
				FQuat Q = SampleRotationBundle(RB, B.Codec, t, B.NumTracks, Time);
				Out->SetRotation(Q);
				bFoundRotation = TRUE;
				break;
			}
		}
		if (bFoundRotation) break;
	}
	if (!bFoundRotation)
	{
		Out->SetRotation(FQuat::Identity);
	}

	UBOOL bFoundTranslation = FALSE;
	const FBundle* TransBundles = (const FBundle*)&Data[Anim->TranslationScaleBundlesOffset];
	for (INT i = 0; i < Anim->NumTranslationScaleBundles; i++)
	{
		const FBundle& B = TransBundles[i];
		const BYTE* TrackMap = &Data[B.TracksAndHeadersOffset];
		for (INT t = 0; t < B.NumTracks; t++)
		{
			if (TrackMap[t] == TrackIndex)
			{
				FResolvedBundle RB;
				RB.Resolve(Data, B);
				FCatmullRomTime Time = FCatmullRomTime::Make(NormalizedTime, B.NumFrames);
				FVector V = SampleTranslationBundle(RB, B.Codec, t, B.NumTracks, Time);
				Out->SetTranslation(V);
				bFoundTranslation = TRUE;
				break;
			}
		}
		if (bFoundTranslation) break;
	}
	if (!bFoundTranslation)
	{
		Out->SetTranslation(FVector::ZeroVector);
	}

	Out->SetScale(1.0f);
}

// Root motion lives in its own single-track bundles: a yaw-only quat per frame
// for rotation, absolute translation for translation.
UBOOL AnimZip_Sample_Motion(const UAnimSequence* Seq, FLOAT NormalizedTime, FBoneAtom* Out)
{
	Out->SetRotation(FQuat::Identity);
	Out->SetTranslation(FVector::ZeroVector);
	Out->SetScale(1.0f);

	if (Seq->AnimZip_Data.Num() < (INT)sizeof(FAnim))
	{
		return FALSE;
	}

	const BYTE* Data = Seq->AnimZip_Data.GetData();
	const FAnim* Anim = (const FAnim*)Data;
	const INT RotOffset = Anim->MotionRotationBundleOffset;
	const INT TransOffset = Anim->MotionTranslationScaleBundleOffset;
	if (RotOffset < 0 && TransOffset < 0)
	{
		return FALSE;
	}

	NormalizedTime = Clamp(NormalizedTime, 0.0f, 1.0f - (FLOAT)SMALL_NUMBER);

	if (RotOffset >= 0)
	{
		const FBundle& B = *(const FBundle*)&Data[RotOffset];
		FResolvedBundle RB;
		RB.Resolve(Data, B);
		FCatmullRomTime Time = FCatmullRomTime::Make(NormalizedTime, B.NumFrames);
		Out->SetRotation(SampleRotationBundle(RB, B.Codec, 0, B.NumTracks, Time));
	}

	if (TransOffset >= 0)
	{
		const FBundle& B = *(const FBundle*)&Data[TransOffset];
		FResolvedBundle RB;
		RB.Resolve(Data, B);
		FCatmullRomTime Time = FCatmullRomTime::Make(NormalizedTime, B.NumFrames);
		Out->SetTranslation(SampleTranslationBundle(RB, B.Codec, 0, B.NumTracks, Time));
	}

	return TRUE;
}

void AnimZip_Sample(const UAnimSequence* Seq, USkeletalMesh* SkelMesh,
	FLOAT NormalizedTime, const TArray<INT>& TrackToBoneTable, INT NumBones, FBoneAtom* Out_Bones)
{
	const TArray<FMeshBone>& RefSkel = SkelMesh->RefSkeleton;
	for (INT i = 0; i < NumBones; i++)
	{
		Out_Bones[i].SetComponents(RefSkel(i).BonePos.Orientation, RefSkel(i).BonePos.Position);
		Out_Bones[i].SetScale(1.0f);
	}

	const BYTE* Data = Seq->AnimZip_Data.GetData();
	const FAnim* Anim = (const FAnim*)Data;

	NormalizedTime = Clamp(NormalizedTime, 0.0f, 1.0f - (FLOAT)SMALL_NUMBER);

	// Relative keys are composed back onto the refpose at sample time: RefPose * Decoded for
	// rotation, RefPose + Decoded for translation (Default.xex.c:2981587, :2967216).
	const UBOOL bRetarget = Seq->Compression_RelativeToReferencePose;

	const FBundle* RotBundles = (const FBundle*)&Data[Anim->RotationBundlesOffset];
	for (INT i = 0; i < Anim->NumRotationBundles; i++)
	{
		const FBundle& B = RotBundles[i];
		FResolvedBundle RB;
		RB.Resolve(Data, B);
		FCatmullRomTime Time = FCatmullRomTime::Make(NormalizedTime, B.NumFrames);

		for (INT t = 0; t < B.NumTracks; t++)
		{
			INT AnimTrack = RB.TrackToAnimTrack[t];
			INT BoneIdx = (AnimTrack < TrackToBoneTable.Num()) ? TrackToBoneTable(AnimTrack) : INDEX_NONE;
			if (BoneIdx != INDEX_NONE && BoneIdx < NumBones)
			{
				FQuat Q = SampleRotationBundle(RB, B.Codec, t, B.NumTracks, Time);
				if (bRetarget)
				{
					// Out_Bones[BoneIdx] still holds the pre-filled refpose at this point.
					Q = Out_Bones[BoneIdx].GetRotation() * Q;
				}
				Out_Bones[BoneIdx].SetRotation(Q);
			}
		}
	}

	const FBundle* TransBundles = (const FBundle*)&Data[Anim->TranslationScaleBundlesOffset];
	for (INT i = 0; i < Anim->NumTranslationScaleBundles; i++)
	{
		const FBundle& B = TransBundles[i];
		FResolvedBundle RB;
		RB.Resolve(Data, B);
		FCatmullRomTime Time = FCatmullRomTime::Make(NormalizedTime, B.NumFrames);

		for (INT t = 0; t < B.NumTracks; t++)
		{
			INT AnimTrack = RB.TrackToAnimTrack[t];
			INT BoneIdx = (AnimTrack < TrackToBoneTable.Num()) ? TrackToBoneTable(AnimTrack) : INDEX_NONE;
			if (BoneIdx != INDEX_NONE && BoneIdx < NumBones)
			{
				FVector V = SampleTranslationBundle(RB, B.Codec, t, B.NumTracks, Time);
				if (bRetarget)
				{
					V += Out_Bones[BoneIdx].GetTranslation();
				}
				Out_Bones[BoneIdx].SetTranslation(V);
			}
		}
	}
}

// AnimZip compression: produces AnimZip_Data from RawAnimationData after PSA import.
// QuatMax48 rotations (only tracks differing from the ref pose), NoScaleFloat96 root translation.

// QuatMax48 encoder - inverse of DecodeQuatMax48.
static void EncodeQuatMax48(const FQuat& InQ, BYTE* Out)
{
	FQuat Q = InQ;

	FLOAT AbsVals[4] = { Abs(Q.X), Abs(Q.Y), Abs(Q.Z), Abs(Q.W) };
	INT S = 0;
	FLOAT MaxAbs = AbsVals[0];
	for (INT i = 1; i < 4; i++)
	{
		if (AbsVals[i] > MaxAbs)
		{
			MaxAbs = AbsVals[i];
			S = i;
		}
	}

	// Negate so the omitted (largest) component is positive.
	FLOAT Components[4] = { Q.X, Q.Y, Q.Z, Q.W };
	if (Components[S] < 0.0f)
	{
		Components[0] = -Components[0];
		Components[1] = -Components[1];
		Components[2] = -Components[2];
		Components[3] = -Components[3];
	}

	FLOAT m, h, l;
	switch (S)
	{
	case 0: m = Components[1]; h = Components[2]; l = Components[3]; break; // X omitted
	case 1: m = Components[0]; h = Components[2]; l = Components[3]; break; // Y omitted
	case 2: m = Components[0]; h = Components[1]; l = Components[3]; break; // Z omitted
	default: m = Components[0]; h = Components[1]; l = Components[2]; break; // W omitted
	}

	// Decoder is val * (sqrt(2) / 32767) - 1/sqrt(2), so invert that to quantize to 15 bits.
	static const FLOAT Shift = 0.70710678118f; // 1/sqrt(2)
	static const FLOAT Scale = 32767.0f / 1.41421356237f; // 32767 / sqrt(2)

	INT mVal = Clamp<INT>(appFloor((m + Shift) * Scale + 0.5f), 0, 32767);
	INT hVal = Clamp<INT>(appFloor((h + Shift) * Scale + 0.5f), 0, 32767);
	INT lVal = Clamp<INT>(appFloor((l + Shift) * Scale + 0.5f), 0, 32767);

	// Pack into 6 bytes big-endian, S split across the top bits of b[2] and b[4].
	Out[0] = (BYTE)((mVal >> 8) & 0x7F);
	Out[1] = (BYTE)(mVal & 0xFF);
	Out[2] = (BYTE)(((S & 2) << 6) | ((hVal >> 8) & 0x7F));
	Out[3] = (BYTE)(hVal & 0xFF);
	Out[4] = (BYTE)(((S & 1) << 7) | ((lVal >> 8) & 0x7F));
	Out[5] = (BYTE)(lVal & 0xFF);
}

// QuatMax40 encoder - inverse of DecodeQuatMax40. Three 12-bit components packed
// big-endian across 5 bytes, S in the low 2 bits of b[4]; b[4] bits 3..2 are unused.

static void EncodeQuatMax40(const FQuat& InQ, BYTE* Out)
{
	FQuat Q = InQ;
	FLOAT AbsVals[4] = { Abs(Q.X), Abs(Q.Y), Abs(Q.Z), Abs(Q.W) };
	INT S = 0;
	FLOAT MaxAbs = AbsVals[0];
	for (INT i = 1; i < 4; i++)
	{
		if (AbsVals[i] > MaxAbs) { MaxAbs = AbsVals[i]; S = i; }
	}

	FLOAT Components[4] = { Q.X, Q.Y, Q.Z, Q.W };
	if (Components[S] < 0.0f)
	{
		Components[0] = -Components[0];
		Components[1] = -Components[1];
		Components[2] = -Components[2];
		Components[3] = -Components[3];
	}

	FLOAT m, h, l;
	switch (S)
	{
	case 0: m = Components[1]; h = Components[2]; l = Components[3]; break;
	case 1: m = Components[0]; h = Components[2]; l = Components[3]; break;
	case 2: m = Components[0]; h = Components[1]; l = Components[3]; break;
	default: m = Components[0]; h = Components[1]; l = Components[2]; break;
	}

	static const FLOAT Shift = 0.70710678118f;
	static const FLOAT Scale = 4095.0f / 1.41421356237f;

	INT mVal = Clamp<INT>(appFloor((m + Shift) * Scale + 0.5f), 0, 4095);
	INT hVal = Clamp<INT>(appFloor((h + Shift) * Scale + 0.5f), 0, 4095);
	INT lVal = Clamp<INT>(appFloor((l + Shift) * Scale + 0.5f), 0, 4095);

	Out[0] = (BYTE)((mVal >> 4) & 0xFF);
	Out[1] = (BYTE)(((mVal & 0xF) << 4) | ((hVal >> 8) & 0xF));
	Out[2] = (BYTE)(hVal & 0xFF);
	Out[3] = (BYTE)((lVal >> 4) & 0xFF);
	Out[4] = (BYTE)(((lVal & 0xF) << 4) | (BYTE)(S & 3));
}

// Interval_Fixed_48 translation encoder. Header is 6 floats (center, half-extent);
// each keyframe is 3 SHORTs decoded as vi/32767 * HalfExtent + Center.

static void ComputeIntervalHeader48(const TArray<FVector>& Samples, FLOAT* Hdr)
{
	FVector Mn = Samples(0), Mx = Samples(0);
	for (INT i = 1; i < Samples.Num(); i++)
	{
		const FVector& V = Samples(i);
		Mn.X = Min(Mn.X, V.X); Mx.X = Max(Mx.X, V.X);
		Mn.Y = Min(Mn.Y, V.Y); Mx.Y = Max(Mx.Y, V.Y);
		Mn.Z = Min(Mn.Z, V.Z); Mx.Z = Max(Mx.Z, V.Z);
	}
	Hdr[0] = 0.5f * (Mn.X + Mx.X);
	Hdr[1] = 0.5f * (Mn.Y + Mx.Y);
	Hdr[2] = 0.5f * (Mn.Z + Mx.Z);
	Hdr[3] = Max(0.5f * (Mx.X - Mn.X), (FLOAT)KINDA_SMALL_NUMBER);
	Hdr[4] = Max(0.5f * (Mx.Y - Mn.Y), (FLOAT)KINDA_SMALL_NUMBER);
	Hdr[5] = Max(0.5f * (Mx.Z - Mn.Z), (FLOAT)KINDA_SMALL_NUMBER);
}

static void EncodeIntervalFixed48Key(const FVector& V, const FLOAT* Hdr, BYTE* Out)
{
	SHORT* Dst = (SHORT*)Out;
	Dst[0] = (SHORT)Clamp<INT>(appRound((V.X - Hdr[0]) / Hdr[3] * 32767.0f), -32767, 32767);
	Dst[1] = (SHORT)Clamp<INT>(appRound((V.Y - Hdr[1]) / Hdr[4] * 32767.0f), -32767, 32767);
	Dst[2] = (SHORT)Clamp<INT>(appRound((V.Z - Hdr[2]) / Hdr[5] * 32767.0f), -32767, 32767);
}

// 1:1 port of AnimZip_ShouldAutoDeleteTrackBasedOnAnimSet (BmGame.exe.c:11493726).
// Our UAnimSet schema doesn't expose the gating bit flags yet, so this always returns FALSE.
static UBOOL AnimZip_ShouldAutoDeleteTrack(FName BoneName, UAnimSet* /*AnimSet*/)
{
	return FALSE;
}

// 1:1 port of GetMaxRotationError (BmGame.exe.c:11476278). Max chord-length over the
// track, which approximates radians for small angles; the caller converts to degrees.
static FLOAT AnimZip_GetMaxRotationError(const FQuat& Ref, const TArray<FQuat>& Keys)
{
	FLOAT MaxErr = 0.0f;
	for (INT i = 0; i < Keys.Num(); i++)
	{
		const FQuat& K = Keys(i);
		const FLOAT D0 = Ref.X - K.X, D1 = Ref.Y - K.Y, D2 = Ref.Z - K.Z, D3 = Ref.W - K.W;
		const FLOAT S0 = Ref.X + K.X, S1 = Ref.Y + K.Y, S2 = Ref.Z + K.Z, S3 = Ref.W + K.W;
		const FLOAT SumSq  = S0*S0 + S1*S1 + S2*S2 + S3*S3;
		const FLOAT DiffSq = D0*D0 + D1*D1 + D2*D2 + D3*D3;
		const FLOAT SqDist = Min(SumSq, DiffSq);
		const FLOAT Dist   = 2.0f * appSqrt(SqDist);
		if (Dist > MaxErr) MaxErr = Dist;
	}
	return MaxErr;
}

// 1:1 port of GetMaxTranslationError (BmGame.exe.c:11476684).
static FLOAT AnimZip_GetMaxTranslationError(const FVector& Ref, const TArray<FVector>& Keys)
{
	FLOAT MaxSq = 0.0f;
	for (INT i = 0; i < Keys.Num(); i++)
	{
		const FVector& K = Keys(i);
		const FLOAT D = (Ref.X - K.X) * (Ref.X - K.X)
		              + (Ref.Y - K.Y) * (Ref.Y - K.Y)
		              + (Ref.Z - K.Z) * (Ref.Z - K.Z);
		if (D > MaxSq) MaxSq = D;
	}
	return appSqrt(MaxSq);
}

// AnimZip encoder, matching the original RAnimZip_Encode.cpp behavior.

struct FIntermediateRotationTrack
{
	BYTE			AnimTrackIndex;	// index into UAnimSequence::RawAnimationData
	BYTE			Codec;			// EAnimZipRotationCodec once selected
	INT				NumFrames;		// post-downsample
	TArray<FQuat>	Samples;		// massaged, pre-encode keys (length == NumFrames)
	TArray<BYTE>	Header;			// codec-specific per-track header bytes
	TArray<BYTE>	EncodedKeys;	// codec-specific per-frame bytes (KeySize * NumFrames)
};

struct FIntermediateTransScaleTrack
{
	BYTE				AnimTrackIndex;
	BYTE				Codec;		// EAnimZipTranslationScaleCodec once selected
	INT					NumFrames;
	TArray<FVector>		Samples;	// translation only for now (no scale in scoped codec set)
	TArray<BYTE>		Header;
	TArray<BYTE>		EncodedKeys;
};

struct FIntermediateBundle
{
	BYTE		Codec;
	INT			NumFrames;
	TArray<INT>	TrackIndices;		// indices into FIntermediateRotationTrack[] or FIntermediateTransScaleTrack[]
};

struct FResolvedTrackSettings
{
	FAnimZipErrorBounds	ErrorBounds;
	UBOOL				bAllowRotationRetargeting;
	UBOOL				bIsMotion;
	UBOOL				bAutoDelete;	// cape-dummy / eye-FX bones, etc.
};

// Resolve the effective URAnimZip_Settings for a sequence: sequence-level override
// takes precedence, then the owning AnimSet's override, then the CDO.
static URAnimZip_Settings* AnimZip_GetEffectiveSettings(UAnimSequence* Seq)
{
	if (Seq && Seq->Compression_CustomSettings)
	{
		return Seq->Compression_CustomSettings;
	}
	UAnimSet* AnimSet = Seq ? Seq->GetAnimSet() : NULL;
	if (AnimSet && AnimSet->Compression_CustomSettings)
	{
		return AnimSet->Compression_CustomSettings;
	}
	return URAnimZip_Settings::StaticClass()->GetDefaultObject<URAnimZip_Settings>();
}

// Case-insensitive prefix match, used by the bone-name pattern matchers (sub_23FB070 and co).
static UBOOL AnimZip_BoneNameStartsWith(const FString& Name, const TCHAR* Prefix)
{
	const INT PrefixLen = appStrlen(Prefix);
	if (Name.Len() < PrefixLen) return FALSE;
	return appStrnicmp(*Name, Prefix, PrefixLen) == 0;
}

// 1:1 port of sub_23FB530 (BmGame.exe.c:11489328). Picks the per-bone FAnimZipTrackSettings,
// then GetTrackSettings scales ErrorBounds by CompressionAmount.
static FResolvedTrackSettings AnimZip_ResolveTrackSettings(
	URAnimZip_Settings* Settings, FName BoneName, UBOOL bRootIsBip01, UBOOL bRootIsCape, UBOOL bIsMotion)
{
	FResolvedTrackSettings Out;
	Out.bIsMotion    = bIsMotion;
	Out.bAutoDelete  = FALSE;

	if (bIsMotion)
	{
		const FAnimZipTrackSettings& M = Settings->MotionTrackSettings;
		Out.ErrorBounds               = M.ErrorBounds;
		Out.bAllowRotationRetargeting = M.AllowRotationRetargeting;
	}
	else
	{
		// First scan the per-AnimSet ForcedTrackSettings array.
		const FAnimZipTrackSettings* Picked = NULL;
		for (INT i = 0; i < Settings->ForcedTrackSettings.Num(); i++)
		{
			if (Settings->ForcedTrackSettings(i).TrackName == BoneName)
			{
				Picked = &Settings->ForcedTrackSettings(i).Settings;
				break;
			}
		}

		if (!Picked)
		{
			if (bRootIsCape && Settings->EnableCapeOptimisations)
			{
				Picked = &Settings->CapeTrackSettings;
			}
			else if (!bRootIsBip01 || !Settings->EnableCharacterOptimisations)
			{
				Picked = &Settings->DefaultTrackSettings;
			}
			else
			{
				// Character cascade — order matches sub_23FB530 verbatim.
				const FString& N = BoneName.ToString();
				if      (BoneName == FName(TEXT("Bip01")))                    Picked = &Settings->CharacterTrackSettings_Bip01;
				else if (BoneName == FName(TEXT("Bip01_Pelvis")))             Picked = &Settings->CharacterTrackSettings_Pelvis;
				else if (AnimZip_BoneNameStartsWith(N, TEXT("Bip01_Spine")))  Picked = &Settings->CharacterTrackSettings_Spine;
				else if (AnimZip_BoneNameStartsWith(N, TEXT("FcFx")))         Picked = &Settings->CharacterTrackSettings_Face;
				else if (BoneName == FName(TEXT("Bip01_Head")))               Picked = &Settings->CharacterTrackSettings_Head;
				else if (BoneName == FName(TEXT("Bip01_L_Clavicle"))
				      || BoneName == FName(TEXT("Bip01_R_Clavicle")))         Picked = &Settings->CharacterTrackSettings_Clavicle;
				else if (BoneName == FName(TEXT("Bip01_L_UpperArm"))
				      || BoneName == FName(TEXT("Bip01_L_Forearm"))
				      || BoneName == FName(TEXT("Bip01_R_UpperArm"))
				      || BoneName == FName(TEXT("Bip01_R_Forearm")))          Picked = &Settings->CharacterTrackSettings_Arm;
				else if (BoneName == FName(TEXT("Bip01_L_Hand"))
				      || BoneName == FName(TEXT("Bip01_R_Hand")))             Picked = &Settings->CharacterTrackSettings_Hand;
				else if (AnimZip_BoneNameStartsWith(N, TEXT("Bip01_L_Finger"))
				      || AnimZip_BoneNameStartsWith(N, TEXT("Bip01_R_Finger"))) Picked = &Settings->CharacterTrackSettings_Finger;
				else if (AnimZip_BoneNameStartsWith(N, TEXT("Gundummy")))     Picked = &Settings->CharacterTrackSettings_Gundummy;
				else if (BoneName == FName(TEXT("Bip01_L_Thigh"))
				      || BoneName == FName(TEXT("Bip01_L_Calf"))
				      || BoneName == FName(TEXT("Bip01_R_Thigh"))
				      || BoneName == FName(TEXT("Bip01_R_Calf")))             Picked = &Settings->CharacterTrackSettings_Leg;
				else if (BoneName == FName(TEXT("Bip01_L_Foot"))
				      || BoneName == FName(TEXT("Bip01_R_Foot")))             Picked = &Settings->CharacterTrackSettings_Foot;
				else if (AnimZip_BoneNameStartsWith(N, TEXT("Bip01_L_Toe"))
				      || AnimZip_BoneNameStartsWith(N, TEXT("Bip01_R_Toe")))  Picked = &Settings->CharacterTrackSettings_Toe;
				else                                                          Picked = &Settings->CharacterTrackSettings;
			}
		}

		Out.ErrorBounds               = Picked->ErrorBounds;
		Out.bAllowRotationRetargeting = Picked->AllowRotationRetargeting;
	}

	// GetTrackSettings (11489761) scales ErrorBounds (R/T/S) by CompressionAmount.
	const FLOAT C = Settings->CompressionAmount;
	Out.ErrorBounds.Rotation    *= C;
	Out.ErrorBounds.Translation *= C;
	Out.ErrorBounds.Scale       *= C;
	return Out;
}

static void AnimZip_MassageTracks(UAnimSequence* /*Seq*/, TArray<FRawAnimSequenceTrack>& /*OutTracks*/)
{ /* stub */ }
static void AnimZip_ClipTracks(UAnimSequence* /*Seq*/, TArray<FRawAnimSequenceTrack>& /*Tracks*/)
{ /* stub */ }

// 1:1 port of GetMotionTrack (BmGame.exe.c:11456727) and its helpers. The notify-driven
// branches and CentreOfMass aren't ported - we assume no notifies and an empty BoneMass.

// sub_23CBA50 / sub_23CBB40 (no-notify path): time range = [0, BlendOutPoint].
static void AnimZip_URMotion_GetTimeRange(UAnimSequence* Seq, FLOAT& OutStart, FLOAT& OutEnd)
{
	OutStart = 0.0f;
	OutEnd   = Seq->BlendOutPoint;
	if (OutStart > OutEnd)
	{
		OutEnd   = 0.5f * (OutStart + OutEnd);
		OutStart = OutEnd;
	}
}

// RescaleTime (BmGame.exe.c:11386834): saturating linear map of a1 to [a2,a3]→[0,1].
static FLOAT AnimZip_RescaleTime(FLOAT t, FLOAT lo, FLOAT hi)
{
	if (lo > t) return 0.0f;
	if (t > hi) return 1.0f;
	const FLOAT Span = hi - lo;
	if (Span >= 0.00000001f)
	{
		const FLOAT v = (t - lo) / Span;
		if (v >= 1.0f) return 1.0f;
		if (v <= 0.0f) return 0.0f;
		return v;
	}
	return t >= 0.5f ? 1.0f : 0.0f;
}

// EaseInOut (BmGame.exe.c:11387292): 3t^2 - 2t^3, clamped to [0,1].
static FLOAT AnimZip_EaseInOut(FLOAT t)
{
	if (t < 0.0f) t = 0.0f;
	if ((t - 1.0f) < 0.0f) return (3.0f - 2.0f * t) * t * t;
	return 1.0f;
}

// GetWrappedAngle (BmGame.exe.c:11386980): wrap to (-PI, PI].
static FLOAT AnimZip_GetWrappedAngle(FLOAT a)
{
	if (a > 0.0f) return appFmod(a + (FLOAT)PI, 2.0f * (FLOAT)PI) - (FLOAT)PI;
	if (a >= 0.0f) return 0.0f;
	return -(appFmod((FLOAT)PI - a, 2.0f * (FLOAT)PI) - (FLOAT)PI);
}

// GetAngleFromTo (BmGame.exe.c:11387007): wrapped delta from a→b.
static FLOAT AnimZip_GetAngleFromTo(FLOAT a, FLOAT b)
{
	const FLOAT d = b - a;
	if (d > 0.0f) return appFmod(d + (FLOAT)PI, 2.0f * (FLOAT)PI) - (FLOAT)PI;
	if (d >= 0.0f) return 0.0f;
	return -(appFmod((FLOAT)PI - d, 2.0f * (FLOAT)PI) - (FLOAT)PI);
}

// CanMoveInZ (BmGame.exe.c:11383455): true for Flying/Floating/Falling/Ceiling.
static UBOOL AnimZip_CanMoveInZ(BYTE Physics)
{
	return Physics == 1 || Physics == 2 || Physics == 3 || Physics == 4;
}

// GetCollisionOptions (BmGame.exe.c:11383432): pick Middle or End by time.
static FAnimCollisionOptions AnimZip_GetCollisionOptions(UAnimSequence* Seq, FLOAT NormTime)
{
	return (NormTime < Seq->CollisionOptionsOutPoint)
		? Seq->CollisionOptions.Middle
		: Seq->CollisionOptions.End;
}

// Sample Bip01's raw atom at a normalized time. Assumes encoding hasn't mutated RawAnimationData yet.
static void AnimZip_SampleBip01Raw(UAnimSequence* Seq, FLOAT NormTime, FQuat& OutQ, FVector& OutT)
{
	const FRawAnimSequenceTrack& Root = Seq->RawAnimationData(0);
	const INT NumKeys = Seq->NumFrames;
	const FLOAT KeyPos = (NumKeys > 1) ? Clamp(NormTime, 0.0f, 1.0f) * (NumKeys - 1) : 0.0f;
	const INT K1 = Clamp<INT>(appFloor(KeyPos), 0, NumKeys - 1);
	const INT K2 = Min(K1 + 1, NumKeys - 1);
	const FLOAT A = KeyPos - (FLOAT)K1;

	const INT PK1 = (Root.PosKeys.Num() > 1) ? Min(K1, Root.PosKeys.Num() - 1) : 0;
	const INT PK2 = (Root.PosKeys.Num() > 1) ? Min(K2, Root.PosKeys.Num() - 1) : 0;
	OutT = Lerp(Root.PosKeys(PK1), Root.PosKeys(PK2), A);

	const INT RK1 = (Root.RotKeys.Num() > 1) ? Min(K1, Root.RotKeys.Num() - 1) : 0;
	const INT RK2 = (Root.RotKeys.Num() > 1) ? Min(K2, Root.RotKeys.Num() - 1) : 0;
	FQuat Q1 = Root.RotKeys(RK1); Q1.Normalize();
	FQuat Q2 = Root.RotKeys(RK2); Q2.Normalize();
	if ((Q1 | Q2) < 0.0f) Q2 = FQuat(-Q2.X, -Q2.Y, -Q2.Z, -Q2.W);
	OutQ.X = Q1.X + (Q2.X - Q1.X) * A;
	OutQ.Y = Q1.Y + (Q2.Y - Q1.Y) * A;
	OutQ.Z = Q1.Z + (Q2.Z - Q1.Z) * A;
	OutQ.W = Q1.W + (Q2.W - Q1.W) * A;
	OutQ.Normalize();
}

// sub_23ABCA0 (BmGame.exe.c:11424811): Bip01 translation at a normalized time.
static FVector AnimZip_GetBip01Translation(UAnimSequence* Seq, FLOAT NormTime)
{
	FQuat Q; FVector T;
	AnimZip_SampleBip01Raw(Seq, NormTime, Q, T);
	return T;
}

// sub_23B0E30 (BmGame.exe.c:11429726): Bip01 yaw at a normalized time.
static FLOAT AnimZip_GetBip01Yaw(UAnimSequence* Seq, FLOAT NormTime)
{
	FQuat Q; FVector T;
	AnimZip_SampleBip01Raw(Seq, NormTime, Q, T);
	const FQuatRotationTranslationMatrix M(Q, FVector(0, 0, 0));
	const FLOAT YZ = M.M[1][2];
	FLOAT Yaw;
	if (Abs(YZ) <= 0.99000001f)
	{
		Yaw = appAtan2(M.M[1][1], M.M[1][0]) + (FLOAT)PI;
	}
	else
	{
		Yaw = appAtan2(M.M[0][1], M.M[0][0]) - 0.5f * (FLOAT)PI;
	}
	return AnimZip_GetWrappedAngle(Yaw);
}

// sub_23CF6A0 (BmGame.exe.c:11453464): simple URMotion yaw — lerp ReferenceOptions.Start/End yaw.
static FLOAT AnimZip_URMotion_GetYawSimple(UAnimSequence* Seq, FLOAT NormTime)
{
	FLOAT Lo, Hi; AnimZip_URMotion_GetTimeRange(Seq, Lo, Hi);
	const FAnimReferenceOptions& S = Seq->ReferenceOptions.Start;
	const FAnimReferenceOptions& E = Seq->ReferenceOptions.End;
	FLOAT YawS = S.ForwardYaw * (FLOAT)(PI / 180.0);
	if (S.ForwardYawDirection == 1) YawS = -YawS;
	FLOAT YawE = E.ForwardYaw * (FLOAT)(PI / 180.0);
	if (E.ForwardYawDirection == 1) YawE = -YawE;
	return YawS + (YawE - YawS) * AnimZip_RescaleTime(NormTime, Lo, Hi);
}

// sub_23CF760 (BmGame.exe.c:11453487): full URMotion yaw.
static FLOAT AnimZip_URMotion_GetYawFull(UAnimSequence* Seq, FLOAT NormTime)
{
	FLOAT Lo, Hi; AnimZip_URMotion_GetTimeRange(Seq, Lo, Hi);
	const FAnimReferenceOptions& S = Seq->ReferenceOptions.Start;
	const FAnimReferenceOptions& E = Seq->ReferenceOptions.End;
	FLOAT YawS = S.ForwardYaw * (FLOAT)(PI / 180.0);
	if (S.ForwardYawDirection == 1) YawS = -YawS;
	FLOAT YawE = E.ForwardYaw * (FLOAT)(PI / 180.0);
	if (E.ForwardYawDirection == 1) YawE = -YawE;
	const FLOAT BipS = AnimZip_GetBip01Yaw(Seq, Lo);
	const FLOAT BipE = AnimZip_GetBip01Yaw(Seq, Hi);
	const FLOAT DeltaS = AnimZip_GetAngleFromTo(BipS, YawS);
	const FLOAT DeltaE = AnimZip_GetAngleFromTo(BipE, YawE);
	const FLOAT T = AnimZip_RescaleTime(NormTime, Lo, Hi);
	const FLOAT Delta = DeltaS + (DeltaE - DeltaS) * T;
	const FLOAT ClampT = Clamp(NormTime, Lo, Hi);
	return AnimZip_GetBip01Yaw(Seq, ClampT) + Delta;
}

// sub_23CF8B0 (BmGame.exe.c:11453536): URMotion floor height.
static FLOAT AnimZip_URMotion_GetFloorHeight(UAnimSequence* Seq, FLOAT NormTime)
{
	FLOAT Lo, Hi; AnimZip_URMotion_GetTimeRange(Seq, Lo, Hi);
	const FAnimReferencePeriods& Ref = Seq->ReferenceOptions;
	const FLOAT Zs = Ref.Start.AutomaticFloorHeight
		? AnimZip_GetBip01Translation(Seq, Lo).Z - 120.0f
		: Ref.Start.FloorHeight;
	const FLOAT Ze = Ref.End.AutomaticFloorHeight
		? AnimZip_GetBip01Translation(Seq, Hi).Z - 120.0f
		: Ref.End.FloorHeight;
	const FLOAT T = AnimZip_EaseInOut(AnimZip_RescaleTime(NormTime, Lo, Hi));
	FLOAT Z = Zs + (Ze - Zs) * T;
	if (Ref.EnforceMinimumFloorHeight && Z < Ref.MinimumFloorHeight)
	{
		Z = Ref.MinimumFloorHeight;
	}
	return Z;
}

// sub_23CF9D0 (BmGame.exe.c:11453588): URMotion floor offset for CanMoveInZ physics.
static FLOAT AnimZip_URMotion_GetFloorOffsetInZ(UAnimSequence* Seq, FLOAT NormTime)
{
	const FAnimCollisionOptions& Mid = Seq->CollisionOptions.Middle;
	const FLOAT EndT = (Mid.RootMotionTranslationOption == 3 || Mid.RootMotionTranslationOption == 1)
		? 1.0f : Seq->BlendOutPoint;
	FLOAT Lo = 0.0f;
	FLOAT Hi = EndT;
	if (Lo > Hi) { Hi = 0.5f * (Lo + Hi); Lo = Hi; }

	const FAnimReferencePeriods& Ref = Seq->ReferenceOptions;
	const FLOAT OffsetS = Ref.Start.AutomaticFloorHeight
		? -120.0f
		: Ref.Start.FloorHeight - AnimZip_GetBip01Translation(Seq, Lo).Z;
	const FLOAT OffsetE = Ref.End.AutomaticFloorHeight
		? Ref.End.FloorHeight - AnimZip_GetBip01Translation(Seq, Hi).Z
		: -120.0f;
	const FLOAT T = AnimZip_EaseInOut(AnimZip_RescaleTime(NormTime, Lo, Hi));
	const FLOAT Offset = OffsetS + (OffsetE - OffsetS) * T;
	const FLOAT ClampT = Clamp(NormTime, Lo, Hi);
	FLOAT Result = AnimZip_GetBip01Translation(Seq, ClampT).Z + Offset;
	if (Ref.EnforceMinimumFloorHeight && Result < Ref.MinimumFloorHeight)
	{
		Result = Ref.MinimumFloorHeight;
	}
	return Result;
}

// NamedBoneMasses[17] (Default.xex.c:250259 + dynamic_initializer + AllocateNameEntry).
struct FBmNamedBoneMass { const TCHAR* Name; FLOAT Mass; };
static const FBmNamedBoneMass GBmNamedBoneMasses[17] =
{
	{ TEXT("Bip01_Pelvis"),     0.2124f  },
	{ TEXT("Bip01_Spine1"),     0.1062f  },
	{ TEXT("Bip01_Spine2"),     0.1062f  },
	{ TEXT("Bip01_Spine3"),     0.2832f  },
	{ TEXT("Bip01_Head"),       0.081f   },
	{ TEXT("Bip01_L_UpperArm"), 0.007f   },
	{ TEXT("Bip01_L_Forearm"),  0.011f   },
	{ TEXT("Bip01_L_Hand"),     0.007f   },
	{ TEXT("Bip01_L_Thigh"),    0.025f   },
	{ TEXT("Bip01_L_Calf"),     0.03675f },
	{ TEXT("Bip01_L_Foot"),     0.01925f },
	{ TEXT("Bip01_R_UpperArm"), 0.007f   },
	{ TEXT("Bip01_R_Forearm"),  0.011f   },
	{ TEXT("Bip01_R_Hand"),     0.007f   },
	{ TEXT("Bip01_R_Thigh"),    0.025f   },
	{ TEXT("Bip01_R_Calf"),     0.03675f },
	{ TEXT("Bip01_R_Foot"),     0.01925f },
};

struct FBmBoneMass { INT TrackIndex; FLOAT Mass; };

// sub_23B1090 / sub_23B1020: for each track, walk RefMesh's parents up to the next tracked bone.
static void AnimZip_BuildParentChain(UAnimSet* AnimSet, USkeletalMesh* RefMesh, TArray<INT>& OutParents)
{
	const INT N = AnimSet->TrackBoneNames.Num();
	OutParents.Empty(N); OutParents.AddZeroed(N);
	for (INT i = 0; i < N; i++) { OutParents(i) = -1; }
	if (!RefMesh) { return; }

	TArray<INT> Track2Mesh; Track2Mesh.Empty(N); Track2Mesh.AddZeroed(N);
	for (INT i = 0; i < N; i++)
	{
		Track2Mesh(i) = RefMesh->MatchRefBone(AnimSet->TrackBoneNames(i));
	}

	for (INT i = 0; i < N; i++)
	{
		const INT MeshIdx = Track2Mesh(i);
		if (MeshIdx <= 0) { continue; }
		INT P = RefMesh->RefSkeleton(MeshIdx).ParentIndex;
		while (P >= 0)
		{
			const FName PName = RefMesh->RefSkeleton(P).Name;
			const INT TrackIdx = AnimSet->TrackBoneNames.FindItemIndex(PName);
			if (TrackIdx != INDEX_NONE) { OutParents(i) = TrackIdx; break; }
			if (P == 0) { break; }
			P = RefMesh->RefSkeleton(P).ParentIndex;
		}
	}
}

// sub_23C5180 + sub_23D2AC0: resolve NamedBoneMasses, sum Total mass.
static void AnimZip_ResolveBoneMasses(UAnimSet* AnimSet, TArray<FBmBoneMass>& OutMasses, FLOAT& OutTotalMass)
{
	OutMasses.Empty(17);
	OutTotalMass = 0.0f;
	for (INT i = 0; i < 17; i++)
	{
		const INT Idx = AnimSet->TrackBoneNames.FindItemIndex(FName(GBmNamedBoneMasses[i].Name));
		if (Idx == INDEX_NONE) { continue; }
		FBmBoneMass M; M.TrackIndex = Idx; M.Mass = GBmNamedBoneMasses[i].Mass;
		OutMasses.AddItem(M);
		OutTotalMass += GBmNamedBoneMasses[i].Mass;
	}
}

// Sample raw track i's local atom at NormTime.
static void AnimZip_SampleLocalAtom(const FRawAnimSequenceTrack& Track, INT NumKeys, FLOAT NormTime, FBoneAtom& Out)
{
	const FLOAT KeyPos = (NumKeys > 1) ? Clamp(NormTime, 0.0f, 1.0f) * (NumKeys - 1) : 0.0f;
	const INT K1 = Clamp<INT>(appFloor(KeyPos), 0, NumKeys - 1);
	const INT K2 = Min(K1 + 1, NumKeys - 1);
	const FLOAT A = KeyPos - (FLOAT)K1;

	FVector T(0, 0, 0);
	if (Track.PosKeys.Num() > 0)
	{
		const INT P1 = Min(K1, Track.PosKeys.Num() - 1);
		const INT P2 = Min(K2, Track.PosKeys.Num() - 1);
		T = Lerp(Track.PosKeys(P1), Track.PosKeys(P2), A);
	}

	FQuat Q(0, 0, 0, 1);
	if (Track.RotKeys.Num() > 0)
	{
		const INT R1 = Min(K1, Track.RotKeys.Num() - 1);
		const INT R2 = Min(K2, Track.RotKeys.Num() - 1);
		FQuat Q1 = Track.RotKeys(R1);
		FQuat Q2 = Track.RotKeys(R2);
		if ((Q1 | Q2) < 0.0f) { Q2 = FQuat(-Q2.X, -Q2.Y, -Q2.Z, -Q2.W); }
		Q.X = Q1.X + (Q2.X - Q1.X) * A;
		Q.Y = Q1.Y + (Q2.Y - Q1.Y) * A;
		Q.Z = Q1.Z + (Q2.Z - Q1.Z) * A;
		Q.W = Q1.W + (Q2.W - Q1.W) * A;
		Q.Normalize();
	}
	Out = FBoneAtom(Q, T, 1.0f);
}

// sub_23B1460: SpaceBase[i] = Local[i] * SpaceBase[Parent[i]]. Parents must precede children.
static void AnimZip_ComposeSpaceBases(UAnimSequence* Seq, const TArray<INT>& Parents, FLOAT NormTime, TArray<FBoneAtom>& OutSpaceBases)
{
	const INT N = Parents.Num();
	const INT NumKeys = Seq->NumFrames;
	const INT NumTracks = Seq->RawAnimationData.Num();
	OutSpaceBases.Empty(N); OutSpaceBases.AddZeroed(N);

	for (INT i = 0; i < N; i++)
	{
		FBoneAtom Local;
		if (i < NumTracks) { AnimZip_SampleLocalAtom(Seq->RawAnimationData(i), NumKeys, NormTime, Local); }
		else { Local = FBoneAtom::Identity; }

		// ActorX W-flip for non-root bones (BmGame.exe.c:11456469, mask = (1,1,1,-1)).
		if (i != 0)
		{
			FQuat Q = Local.GetRotation();
			Local.SetRotation(FQuat(Q.X, Q.Y, Q.Z, -Q.W));
		}

		const INT P = Parents(i);
		if (P >= 0 && P < i) { OutSpaceBases(i) = Local * OutSpaceBases(P); }
		else                 { OutSpaceBases(i) = Local; }
	}
}

// sub_23D2C10 + GetCentreOfMass: per-frame CoM array.
static void AnimZip_BuildCentreOfMassArray(
	UAnimSequence* Seq, UAnimSet* AnimSet, USkeletalMesh* RefMesh,
	TArray<FVector>& OutCoM)
{
	const INT NumFrames = Seq->NumFrames;
	OutCoM.Empty(NumFrames); OutCoM.AddZeroed(NumFrames);

	TArray<FBmBoneMass> Masses; FLOAT TotalMass = 0.0f;
	AnimZip_ResolveBoneMasses(AnimSet, Masses, TotalMass);
	if (Masses.Num() == 0 || TotalMass <= 0.00000001f || !RefMesh) { return; }

	TArray<INT> Parents;
	AnimZip_BuildParentChain(AnimSet, RefMesh, Parents);

	const FLOAT InvTotal = 1.0f / TotalMass;
	TArray<FBoneAtom> SpaceBases;
	for (INT f = 0; f < NumFrames; f++)
	{
		const FLOAT NormTime = (NumFrames > 1) ? (FLOAT)f / (FLOAT)(NumFrames - 1) : 0.0f;
		AnimZip_ComposeSpaceBases(Seq, Parents, NormTime, SpaceBases);

		FVector Sum(0, 0, 0);
		for (INT k = 0; k < Masses.Num(); k++)
		{
			const INT TI = Masses(k).TrackIndex;
			if (TI >= 0 && TI < SpaceBases.Num()) { Sum += SpaceBases(TI).GetTranslation() * Masses(k).Mass; }
		}
		OutCoM(f) = Sum * InvTotal;
	}
}

// Main 1:1 port of GetMotionTrack (BmGame.exe.c:11456727). Yaw-only quat +
// (XY=v45, Z=URMotion floor) translation per frame.
static void AnimZip_GetMotionTrack(UAnimSequence* Seq, UAnimSet* AnimSet, USkeletalMesh* RefMesh,
	TArray<FQuat>& OutRot, TArray<FVector>& OutTrans)
{
	const INT NumFrames = Seq->NumFrames;
	OutRot.Empty(NumFrames);   OutRot.Add(NumFrames);
	OutTrans.Empty(NumFrames); OutTrans.Add(NumFrames);

	const BITFIELD UseSimpleYaw = Seq->bUseSimpleForwardYaw;
	const BITFIELD UseSimpleFloor = Seq->bUseSimpleFloorHeight;
	const BITFIELD UseSimpleXY = Seq->bUseSimpleRootMotionXY;

	// sub_23D3160 v45 array. Weight=1 (no MotionExtractionType notify) → v45 = CoM.
	TArray<FVector> V45;
	AnimZip_BuildCentreOfMassArray(Seq, AnimSet, RefMesh, V45);

	for (INT f = 0; f < NumFrames; f++)
	{
		const FLOAT NormTime = (NumFrames > 1) ? (FLOAT)f / (FLOAT)(NumFrames - 1) : 0.0f;
		const FAnimCollisionOptions Col = AnimZip_GetCollisionOptions(Seq, NormTime);

		const FLOAT Yaw = (Col.RootMotionRotationOption == 2 || UseSimpleYaw)
			? AnimZip_URMotion_GetYawSimple(Seq, NormTime)
			: AnimZip_URMotion_GetYawFull(Seq, NormTime);
		const FLOAT Half = 0.5f * Yaw;
		OutRot(f) = FQuat(0.0f, 0.0f, appSin(Half), appCos(Half));

		const FLOAT Z = (Col.RootMotionTranslationOption == 2)
			? AnimZip_URMotion_GetFloorHeight(Seq, NormTime)
			: (UseSimpleFloor || !AnimZip_CanMoveInZ(Col.Physics))
				? AnimZip_URMotion_GetFloorHeight(Seq, NormTime)
				: AnimZip_URMotion_GetFloorOffsetInZ(Seq, NormTime);

		FVector XY(0, 0, 0);
		if (V45.Num() == NumFrames)
		{
			if (UseSimpleXY)
			{
				FLOAT Lo, Hi; AnimZip_URMotion_GetTimeRange(Seq, Lo, Hi);
				const FLOAT T = AnimZip_EaseInOut(AnimZip_RescaleTime(NormTime, Lo, Hi));
				XY = V45(0) + (V45(NumFrames - 1) - V45(0)) * T;
			}
			else
			{
				XY = V45(f);
			}
		}

		FLOAT X = XY.X, Y = XY.Y;
		if (Col.RootMotionTranslationOption == 2) { X = 0.0f; Y = 0.0f; }

		OutTrans(f) = FVector(X, Y, Z);
	}
}

// Build the per-call rotation codec list. 1:1 port of sub_23FFB10, limited to the
// codecs we implement (QuatMax_40, QuatMax_48) and always keeping QuatMax_48 as a fallback.
static void AnimZip_BuildRotationCodecList(URAnimZip_Settings* Cfg, TArray<BYTE>& Out)
{
	Out.Empty();
	if (Cfg->ForceRotationCodec_Enabled)
	{
		const BYTE F = (BYTE)Cfg->ForceRotationCodec;
		if (F == AZRC_QuatMax_40 || F == AZRC_QuatMax_48)
		{
			Out.AddItem(F);
		}
	}
	else
	{
		for (INT v = 6; v >= 0; v--)
		{
			UBOOL bDisabled = FALSE;
			for (INT d = 0; d < Cfg->DisableRotationCodecs.Num(); d++)
			{
				if ((BYTE)Cfg->DisableRotationCodecs(d) == (BYTE)v) { bDisabled = TRUE; break; }
			}
			if (bDisabled) continue;
			if (v == AZRC_QuatMax_40 || v == AZRC_QuatMax_48)
			{
				Out.AddItem((BYTE)v);
			}
		}
	}
	if (Out.Num() == 0)
	{
		Out.AddItem((BYTE)AZRC_QuatMax_48);
	}
}

// Try one rotation codec, accepting it if the round-trip error is within tolerance or bForce is set.
static UBOOL AnimZip_TryRotationCodec(
	BYTE Codec, const TArray<FQuat>& Samples, FLOAT ToleranceDeg,
	UBOOL bForce, FIntermediateRotationTrack& OutTrack)
{
	if (Samples.Num() == 0)
	{
		if (bForce) { OutTrack.Codec = Codec; return TRUE; }
		return FALSE;
	}
	FLOAT MaxErrDeg = 0.0f;
	for (INT i = 0; i < Samples.Num(); i++)
	{
		FQuat Dec;
		BYTE Buf[6];
		if (Codec == AZRC_QuatMax_40)
		{
			EncodeQuatMax40(Samples(i), Buf);
			Dec = DecodeQuatMax40(Buf);
		}
		else // AZRC_QuatMax_48
		{
			EncodeQuatMax48(Samples(i), Buf);
			Dec = DecodeQuatMax48(Buf);
		}
		TArray<FQuat> One; One.AddItem(Dec);
		const FLOAT E = AnimZip_GetMaxRotationError(Samples(i), One) * (180.0f / PI);
		if (E > MaxErrDeg) MaxErrDeg = E;
	}
	if (bForce || MaxErrDeg <= ToleranceDeg)
	{
		OutTrack.Codec = Codec;
		return TRUE;
	}
	return FALSE;
}

// 1:1 port of SelectRotationCodec (BmGame.exe.c:11493389). Smallest codec first, forcing the last.
static void AnimZip_SelectRotationCodec(
	const TArray<FQuat>& Samples, const FResolvedTrackSettings& Settings,
	URAnimZip_Settings* Cfg, FIntermediateRotationTrack& OutTrack)
{
	TArray<BYTE> Codecs;
	AnimZip_BuildRotationCodecList(Cfg, Codecs);

	OutTrack.Codec = 7; // sentinel "unset" matching reference
	for (INT i = 0; i < Codecs.Num(); i++)
	{
		if (AnimZip_TryRotationCodec(Codecs(i), Samples, Settings.ErrorBounds.Rotation, FALSE, OutTrack))
		{
			return;
		}
	}
	AnimZip_TryRotationCodec(Codecs(Codecs.Num() - 1), Samples, Settings.ErrorBounds.Rotation, TRUE, OutTrack);
}

// Build the per-call translation/scale codec list. 1:1 port of sub_23FFFA0
// (BmGame.exe.c:11493587) limited to the codecs we implement.
static void AnimZip_BuildTransScaleCodecList(URAnimZip_Settings* Cfg, TArray<BYTE>& Out)
{
	Out.Empty();
	if (Cfg->ForceTranslationScaleCodec_Enabled)
	{
		const BYTE F = (BYTE)Cfg->ForceTranslationScaleCodec;
		if (F == AZTSC_NoScale_Float_96 || F == AZTSC_NoScale_Interval_Fixed_48)
		{
			Out.AddItem(F);
		}
	}
	else
	{
		for (INT v = 3; v >= 0; v--)
		{
			UBOOL bDisabled = FALSE;
			for (INT d = 0; d < Cfg->DisableTranslationScaleCodecs.Num(); d++)
			{
				if ((BYTE)Cfg->DisableTranslationScaleCodecs(d) == (BYTE)v) { bDisabled = TRUE; break; }
			}
			if (bDisabled) continue;
			if (v == AZTSC_NoScale_Float_96 || v == AZTSC_NoScale_Interval_Fixed_48)
			{
				Out.AddItem((BYTE)v);
			}
		}
	}
	if (Out.Num() == 0)
	{
		Out.AddItem((BYTE)AZTSC_NoScale_Float_96);
	}
}

static UBOOL AnimZip_TryTransScaleCodec(
	BYTE Codec, const TArray<FVector>& Samples, FLOAT Tolerance,
	UBOOL bForce, FIntermediateTransScaleTrack& OutTrack)
{
	if (Samples.Num() == 0)
	{
		if (bForce) { OutTrack.Codec = Codec; return TRUE; }
		return FALSE;
	}
	FLOAT MaxErr = 0.0f;
	if (Codec == AZTSC_NoScale_Interval_Fixed_48)
	{
		FLOAT Hdr[6];
		ComputeIntervalHeader48(Samples, Hdr);
		BYTE Buf[6];
		for (INT i = 0; i < Samples.Num(); i++)
		{
			EncodeIntervalFixed48Key(Samples(i), Hdr, Buf);
			const SHORT* S = (const SHORT*)Buf;
			const FVector Dec(
				(FLOAT)S[0] / 32767.0f * Hdr[3] + Hdr[0],
				(FLOAT)S[1] / 32767.0f * Hdr[4] + Hdr[1],
				(FLOAT)S[2] / 32767.0f * Hdr[5] + Hdr[2]);
			TArray<FVector> One; One.AddItem(Dec);
			const FLOAT E = AnimZip_GetMaxTranslationError(Samples(i), One);
			if (E > MaxErr) MaxErr = E;
		}
	}
	if (bForce || MaxErr <= Tolerance)
	{
		OutTrack.Codec = Codec;
		return TRUE;
	}
	return FALSE;
}

// 1:1 port of SelectTranslationScaleCodec (BmGame.exe.c:11510526).
static void AnimZip_SelectTransScaleCodec(
	const TArray<FVector>& Samples, const FResolvedTrackSettings& Settings,
	URAnimZip_Settings* Cfg, FIntermediateTransScaleTrack& OutTrack)
{
	TArray<BYTE> Codecs;
	AnimZip_BuildTransScaleCodecList(Cfg, Codecs);

	OutTrack.Codec = 4; // sentinel
	for (INT i = 0; i < Codecs.Num(); i++)
	{
		if (AnimZip_TryTransScaleCodec(Codecs(i), Samples, Settings.ErrorBounds.Translation, FALSE, OutTrack))
		{
			return;
		}
	}
	AnimZip_TryTransScaleCodec(Codecs(Codecs.Num() - 1), Samples, Settings.ErrorBounds.Translation, TRUE, OutTrack);
}

// Per-track encoders: build IT.Header / IT.EncodedKeys for the selected codec.

static void AnimZip_EncodeRotationTrack(FIntermediateRotationTrack& IT)
{
	const INT N = IT.NumFrames;
	IT.Header.Empty();
	switch (IT.Codec)
	{
	case AZRC_QuatMax_48:
		IT.EncodedKeys.Empty(6 * N); IT.EncodedKeys.Add(6 * N);
		for (INT f = 0; f < N; f++)
		{
			EncodeQuatMax48(IT.Samples(f), &IT.EncodedKeys(6 * f));
		}
		break;
	case AZRC_QuatMax_40:
		IT.EncodedKeys.Empty(5 * N); IT.EncodedKeys.Add(5 * N);
		for (INT f = 0; f < N; f++)
		{
			EncodeQuatMax40(IT.Samples(f), &IT.EncodedKeys(5 * f));
		}
		break;
	default:
		checkf(0, TEXT("AnimZip_EncodeRotationTrack: unsupported codec %d"), IT.Codec);
	}
}

static void AnimZip_EncodeTransScaleTrack(FIntermediateTransScaleTrack& IT)
{
	const INT N = IT.NumFrames;
	switch (IT.Codec)
	{
	case AZTSC_NoScale_Float_96:
		IT.Header.Empty();
		IT.EncodedKeys.Empty(12 * N); IT.EncodedKeys.Add(12 * N);
		for (INT f = 0; f < N; f++)
		{
			FLOAT* Dst = (FLOAT*)&IT.EncodedKeys(12 * f);
			Dst[0] = IT.Samples(f).X;
			Dst[1] = IT.Samples(f).Y;
			Dst[2] = IT.Samples(f).Z;
		}
		break;
	case AZTSC_NoScale_Interval_Fixed_48:
	{
		IT.Header.Empty(24); IT.Header.Add(24);
		FLOAT* Hdr = (FLOAT*)IT.Header.GetData();
		ComputeIntervalHeader48(IT.Samples, Hdr);
		IT.EncodedKeys.Empty(6 * N); IT.EncodedKeys.Add(6 * N);
		for (INT f = 0; f < N; f++)
		{
			EncodeIntervalFixed48Key(IT.Samples(f), Hdr, &IT.EncodedKeys(6 * f));
		}
		break;
	}
	default:
		checkf(0, TEXT("AnimZip_EncodeTransScaleTrack: unsupported codec %d"), IT.Codec);
	}
}

static void AnimZip_DownsampleRotation(TArray<FQuat>& /*Samples*/, const FResolvedTrackSettings& /*Settings*/)
{ /* stub: disabled */ }
static void AnimZip_DownsampleTranslation(TArray<FVector>& /*Samples*/, const FResolvedTrackSettings& /*Settings*/)
{ /* stub: disabled */ }

// Group tracks sharing (Codec, NumFrames) into bundles, spilling past 255 (NumTracks is a BYTE).
static void AnimZip_GroupIntoBundles(
	const TArray<FIntermediateRotationTrack>& RotTracks,
	const TArray<FIntermediateTransScaleTrack>& TransTracks,
	TArray<FIntermediateBundle>& OutRotBundles,
	TArray<FIntermediateBundle>& OutTransBundles)
{
	OutRotBundles.Empty();
	for (INT i = 0; i < RotTracks.Num(); i++)
	{
		const FIntermediateRotationTrack& IT = RotTracks(i);
		INT Found = INDEX_NONE;
		for (INT b = 0; b < OutRotBundles.Num(); b++)
		{
			FIntermediateBundle& B = OutRotBundles(b);
			if (B.Codec == IT.Codec && B.NumFrames == IT.NumFrames && B.TrackIndices.Num() < 255)
			{
				Found = b; break;
			}
		}
		if (Found == INDEX_NONE)
		{
			FIntermediateBundle NB;
			NB.Codec = IT.Codec;
			NB.NumFrames = IT.NumFrames;
			OutRotBundles.AddItem(NB);
			Found = OutRotBundles.Num() - 1;
		}
		OutRotBundles(Found).TrackIndices.AddItem(i);
	}

	OutTransBundles.Empty();
	for (INT i = 0; i < TransTracks.Num(); i++)
	{
		const FIntermediateTransScaleTrack& IT = TransTracks(i);
		INT Found = INDEX_NONE;
		for (INT b = 0; b < OutTransBundles.Num(); b++)
		{
			FIntermediateBundle& B = OutTransBundles(b);
			if (B.Codec == IT.Codec && B.NumFrames == IT.NumFrames && B.TrackIndices.Num() < 255)
			{
				Found = b; break;
			}
		}
		if (Found == INDEX_NONE)
		{
			FIntermediateBundle NB;
			NB.Codec = IT.Codec;
			NB.NumFrames = IT.NumFrames;
			OutTransBundles.AddItem(NB);
			Found = OutTransBundles.Num() - 1;
		}
		OutTransBundles(Found).TrackIndices.AddItem(i);
	}
}

void AnimZip_Compress(UAnimSequence* Seq)
{
	if (!Seq || Seq->RawAnimationData.Num() == 0 || Seq->NumFrames == 0)
	{
		return;
	}

	UAnimSet* AnimSet = Seq->GetAnimSet();
	if (!AnimSet)
	{
		return;
	}

	const INT NumTracks = Seq->RawAnimationData.Num();
	const INT NumFrames = Seq->NumFrames;

	check(NumTracks <= 255); // FBundle::NumTracks is BYTE

	USkeletalMesh* RefMesh = NULL;
	if (AnimSet->PreviewSkelMeshName != NAME_None)
	{
		RefMesh = LoadObject<USkeletalMesh>(NULL, *AnimSet->PreviewSkelMeshName.ToString(), NULL, LOAD_None, NULL);
	}

	URAnimZip_Settings* EffSettings = AnimZip_GetEffectiveSettings(Seq);

	// The character and cape cascades need both the matching root bone name and their
	// enable flag (BmGame.exe.c:11489659, 11504846).
	const UBOOL bRootIsBip01 = (NumTracks > 0 && AnimSet->TrackBoneNames(0) == FName(TEXT("Bip01")));
	const UBOOL bRootIsCape  = (NumTracks > 0 && AnimSet->TrackBoneNames(0) == FName(TEXT("Cape_Dummy")));

	// Build per-track resolved settings (1:1 GetTrackSettings).
	TArray<FResolvedTrackSettings> Resolved;
	Resolved.Empty(NumTracks); Resolved.Add(NumTracks);
	for (INT t = 0; t < NumTracks; t++)
	{
		Resolved(t) = AnimZip_ResolveTrackSettings(EffSettings, AnimSet->TrackBoneNames(t), bRootIsBip01, bRootIsCape, FALSE);
	}

	// Strip decisions (1:1 sub_23FBEF0 / sub_23FC070 / sub_23FC1E0).
	TArray<INT> IncludedRotTracks;
	TArray<INT> IncludedTransTracks;
	for (INT t = 0; t < NumTracks; t++)
	{
		FName BoneName = AnimSet->TrackBoneNames(t);
		if (AnimZip_ShouldAutoDeleteTrack(BoneName, AnimSet))
		{
			continue;
		}
		const FRawAnimSequenceTrack& Track = Seq->RawAnimationData(t);
		const FResolvedTrackSettings& RS = Resolved(t);

		// Rotation strip (sub_23FBEF0).
		UBOOL bStripRot = FALSE;
		if (Track.RotKeys.Num() == 0)
		{
			bStripRot = TRUE;
		}
		else if (RefMesh && EffSettings->StripTracksIfSameAsReferencePose)
		{
			INT BoneIdx = RefMesh->MatchRefBone(BoneName);
			if (BoneIdx != INDEX_NONE)
			{
				FQuat RefQuat = RefMesh->RefSkeleton(BoneIdx).BonePos.Orientation;
				if (BoneIdx > 0)
				{
					RefQuat.W = -RefQuat.W; // ActorX convention flip to match RawAnimationData
				}
				const FLOAT ErrDeg = AnimZip_GetMaxRotationError(RefQuat, Track.RotKeys) * (180.0f / PI);
				if (RS.ErrorBounds.Rotation > ErrDeg)
				{
					bStripRot = TRUE;
				}
			}
		}
		if (!bStripRot)
		{
			IncludedRotTracks.AddItem(t);
		}

		// Translation strip (sub_23FC070).
		UBOOL bStripTrans = FALSE;
		if (Track.PosKeys.Num() == 0)
		{
			bStripTrans = TRUE;
		}
		else if (RefMesh && EffSettings->StripTracksIfSameAsReferencePose)
		{
			INT BoneIdx = RefMesh->MatchRefBone(BoneName);
			if (BoneIdx != INDEX_NONE)
			{
				const FVector& RefPos = RefMesh->RefSkeleton(BoneIdx).BonePos.Position;
				const FLOAT Err = AnimZip_GetMaxTranslationError(RefPos, Track.PosKeys);
				if (RS.ErrorBounds.Translation > Err)
				{
					bStripTrans = TRUE;
				}
			}
		}
		if (!bStripTrans)
		{
			IncludedTransTracks.AddItem(t);
		}
	}

	const INT NRot = IncludedRotTracks.Num();
	const INT NTrans = IncludedTransTracks.Num();

	const FRawAnimSequenceTrack& RootTrack = Seq->RawAnimationData(0);
	// AnimZip_Encode (11512575): GetMotionTrack only fires when TrackBoneNames[0] == FName("Bip01").
	const UBOOL bHasMotionRot   = bRootIsBip01 && (RootTrack.RotKeys.Num() > 0);
	const UBOOL bHasMotionTrans = bRootIsBip01 && (RootTrack.PosKeys.Num() > 0);

	// Build intermediate rotation tracks.

	TArray<FIntermediateRotationTrack> RotInterm;
	RotInterm.Empty(NRot);
	for (INT i = 0; i < NRot; i++)
	{
		INT AnimTrack = IncludedRotTracks(i);
		const FRawAnimSequenceTrack& Track = Seq->RawAnimationData(AnimTrack);
		FName BoneName = AnimSet->TrackBoneNames(AnimTrack);
		FIntermediateRotationTrack IT;
		IT.AnimTrackIndex = (BYTE)AnimTrack;
		IT.NumFrames = NumFrames;
		IT.Samples.Empty(NumFrames);
		for (INT f = 0; f < NumFrames; f++)
		{
			INT KeyIdx = (Track.RotKeys.Num() > 1) ? Min(f, Track.RotKeys.Num() - 1) : 0;
			FQuat Q = Track.RotKeys(KeyIdx);
			// MassageTracks (BmGame.exe.c:11505060): W-flipped for non-root tracks.
			if (AnimTrack > 0)
			{
				Q.W = -Q.W;
			}
			Q.Normalize();
			// Keep the imported sign sequence intact - the sampler aligns hemispheres per sample,
			// and a global pass here would change the interpolated path (visible on Bip01 yaw).
			IT.Samples.AddItem(Q);
		}

		const FResolvedTrackSettings& RS = Resolved(AnimTrack);
		AnimZip_SelectRotationCodec(IT.Samples, RS, EffSettings, IT);
		AnimZip_EncodeRotationTrack(IT);
		RotInterm.AddItem(IT);
	}

	// Build intermediate translation tracks.
	TArray<FIntermediateTransScaleTrack> TransInterm;
	TransInterm.Empty(NTrans);
	for (INT i = 0; i < NTrans; i++)
	{
		INT AnimTrack = IncludedTransTracks(i);
		const FRawAnimSequenceTrack& Track = Seq->RawAnimationData(AnimTrack);

		FIntermediateTransScaleTrack IT;
		IT.AnimTrackIndex = (BYTE)AnimTrack;
		IT.NumFrames = NumFrames;
		IT.Samples.Empty(NumFrames);
		for (INT f = 0; f < NumFrames; f++)
		{
			INT KeyIdx = (Track.PosKeys.Num() > 1) ? Min(f, Track.PosKeys.Num() - 1) : 0;
			IT.Samples.AddItem(Track.PosKeys(KeyIdx));
		}

		const FResolvedTrackSettings& RS = Resolved(AnimTrack);
		AnimZip_SelectTransScaleCodec(IT.Samples, RS, EffSettings, IT);
		AnimZip_EncodeTransScaleTrack(IT);
		TransInterm.AddItem(IT);
	}

	// Must precede AnimZip_GetMotionTrack, which reads BlendOutPoint as the URMotion period
	// upper bound in normalized time. Leaving these at zero collapses NormTime and the motion track.
	Seq->ClippedStart = 0.0f;
	Seq->ClippedLength = Seq->SequenceLength;
	Seq->BlendInPoint = 0.0f;
	Seq->BlendOutPoint = 1.0f;

	// Motion bundles (GetMotionTrack BmGame.exe.c:11456727 + codec select 11512860).
	FResolvedTrackSettings MotionRS = AnimZip_ResolveTrackSettings(
		EffSettings, FName(TEXT("Motion")), bRootIsBip01, bRootIsCape, TRUE);

	TArray<FQuat> MotionRotSamples;
	TArray<FVector> MotionTransSamples;
	if (bHasMotionRot || bHasMotionTrans)
	{
		AnimZip_GetMotionTrack(Seq, AnimSet, RefMesh, MotionRotSamples, MotionTransSamples);
	}

	FIntermediateRotationTrack MotionRotIT;
	if (bHasMotionRot)
	{
		MotionRotIT.AnimTrackIndex = 0;
		MotionRotIT.NumFrames = NumFrames;
		MotionRotIT.Samples = MotionRotSamples;
		AnimZip_SelectRotationCodec(MotionRotIT.Samples, MotionRS, EffSettings, MotionRotIT);
		AnimZip_EncodeRotationTrack(MotionRotIT);
	}

	FIntermediateTransScaleTrack MotionTransIT;
	if (bHasMotionTrans)
	{
		MotionTransIT.AnimTrackIndex = 0;
		MotionTransIT.NumFrames = NumFrames;
		MotionTransIT.Samples = MotionTransSamples;
		AnimZip_SelectTransScaleCodec(MotionTransIT.Samples, MotionRS, EffSettings, MotionTransIT);
		AnimZip_EncodeTransScaleTrack(MotionTransIT);
	}

	TArray<FIntermediateBundle> RotBundles;
	TArray<FIntermediateBundle> TransBundles;
	AnimZip_GroupIntoBundles(RotInterm, TransInterm, RotBundles, TransBundles);

	const INT NumRotBundles = RotBundles.Num();
	const INT NumTransBundles = TransBundles.Num();

	// Compute layout offsets.
	const INT AnimHeaderSize = 24; // sizeof(FAnim)
	const INT BundleSize = 12;     // sizeof(FBundle) packed

	INT Cursor = AnimHeaderSize;

	// Motion bundles come first, stored as delta-from-first-frame. The decoder samples at t=0
	// and composes onto the base pose, so absolute poses would double-apply the root's rest offset.
	const INT MotionRotBundleOffset = bHasMotionRot ? Cursor : -1;
	if (bHasMotionRot) Cursor += BundleSize;
	const INT MotionTransBundleOffset = bHasMotionTrans ? Cursor : -1;
	if (bHasMotionTrans) Cursor += BundleSize;

	const INT RotBundlesOffset = Cursor;
	Cursor += NumRotBundles * BundleSize;
	const INT TransBundlesOffset = Cursor;
	Cursor += NumTransBundles * BundleSize;

	INT MotionRotTrackMapOffset = -1, MotionRotHeaderOffset = -1, MotionRotKeyframesOffset = -1;
	if (bHasMotionRot)
	{
		const INT KeySize = GRotationKeyframeSizes[MotionRotIT.Codec];
		const INT HdrSize = GRotationHeaderSizes[MotionRotIT.Codec];
		MotionRotTrackMapOffset = Cursor; Cursor += 1;
		MotionRotHeaderOffset = Cursor; Cursor += HdrSize;
		MotionRotKeyframesOffset = Cursor; Cursor += KeySize * NumFrames;
	}
	INT MotionTransTrackMapOffset = -1, MotionTransHeaderOffset = -1, MotionTransKeyframesOffset = -1;
	if (bHasMotionTrans)
	{
		const INT KeySize = GTranslationKeyframeSizes[MotionTransIT.Codec];
		const INT HdrSize = GTranslationHeaderSizes[MotionTransIT.Codec];
		MotionTransTrackMapOffset = Cursor; Cursor += 1;
		MotionTransHeaderOffset = Cursor; Cursor += HdrSize;
		MotionTransKeyframesOffset = Cursor; Cursor += KeySize * NumFrames;
	}

	TArray<INT> RotBundleTrackMapOffsets; RotBundleTrackMapOffsets.Empty(NumRotBundles);
	TArray<INT> RotBundleKeyframesOffsets; RotBundleKeyframesOffsets.Empty(NumRotBundles);
	for (INT b = 0; b < NumRotBundles; b++)
	{
		const FIntermediateBundle& B = RotBundles(b);
		const INT NT = B.TrackIndices.Num();
		const INT KeySize = GRotationKeyframeSizes[B.Codec];
		const INT HdrSize = GRotationHeaderSizes[B.Codec];

		RotBundleTrackMapOffsets.AddItem(Cursor);
		Cursor += NT;              // track map
		Cursor += NT * HdrSize;    // per-track headers
		RotBundleKeyframesOffsets.AddItem(Cursor);
		Cursor += NT * B.NumFrames * KeySize;
	}

	TArray<INT> TransBundleTrackMapOffsets; TransBundleTrackMapOffsets.Empty(NumTransBundles);
	TArray<INT> TransBundleKeyframesOffsets; TransBundleKeyframesOffsets.Empty(NumTransBundles);
	for (INT b = 0; b < NumTransBundles; b++)
	{
		const FIntermediateBundle& B = TransBundles(b);
		const INT NT = B.TrackIndices.Num();
		const INT KeySize = GTranslationKeyframeSizes[B.Codec];
		const INT HdrSize = GTranslationHeaderSizes[B.Codec];

		TransBundleTrackMapOffsets.AddItem(Cursor);
		Cursor += NT;
		Cursor += NT * HdrSize;
		TransBundleKeyframesOffsets.AddItem(Cursor);
		Cursor += NT * B.NumFrames * KeySize;
	}

	const INT TotalSize = Cursor;

	Seq->AnimZip_Data.Empty(TotalSize);
	Seq->AnimZip_Data.Add(TotalSize);
	BYTE* Data = Seq->AnimZip_Data.GetData();
	appMemzero(Data, TotalSize);

	FAnim* Anim = (FAnim*)Data;
	Anim->MotionRotationBundleOffset = MotionRotBundleOffset;
	Anim->MotionTranslationScaleBundleOffset = MotionTransBundleOffset;
	Anim->NumRotationBundles = NumRotBundles;
	Anim->RotationBundlesOffset = RotBundlesOffset;
	Anim->NumTranslationScaleBundles = NumTransBundles;
	Anim->TranslationScaleBundlesOffset = TransBundlesOffset;

	// Rotation is yaw-only (BmGame.exe.c:11456887) - GetRootMotion treats pitch/roll as noise,
	// so strip them here.
	if (bHasMotionRot)
	{
		FBundle* MB = (FBundle*)&Data[MotionRotBundleOffset];
		MB->Codec = MotionRotIT.Codec;
		MB->NumTracks = 1;
		MB->NumFrames = (USHORT)NumFrames;
		MB->TracksAndHeadersOffset = MotionRotTrackMapOffset;
		MB->KeyframesOffset = MotionRotKeyframesOffset;
		Data[MotionRotTrackMapOffset] = 0;

		const INT HdrSize = GRotationHeaderSizes[MotionRotIT.Codec];
		if (HdrSize > 0 && MotionRotIT.Header.Num() == HdrSize)
		{
			appMemcpy(&Data[MotionRotHeaderOffset], MotionRotIT.Header.GetData(), HdrSize);
		}
		const INT KeySize = GRotationKeyframeSizes[MotionRotIT.Codec];
		appMemcpy(&Data[MotionRotKeyframesOffset], MotionRotIT.EncodedKeys.GetData(), KeySize * NumFrames);
	}
	if (bHasMotionTrans)
	{
		FBundle* MB = (FBundle*)&Data[MotionTransBundleOffset];
		MB->Codec = MotionTransIT.Codec;
		MB->NumTracks = 1;
		MB->NumFrames = (USHORT)NumFrames;
		MB->TracksAndHeadersOffset = MotionTransTrackMapOffset;
		MB->KeyframesOffset = MotionTransKeyframesOffset;
		Data[MotionTransTrackMapOffset] = 0;

		const INT HdrSize = GTranslationHeaderSizes[MotionTransIT.Codec];
		if (HdrSize > 0 && MotionTransIT.Header.Num() == HdrSize)
		{
			appMemcpy(&Data[MotionTransHeaderOffset], MotionTransIT.Header.GetData(), HdrSize);
		}
		const INT KeySize = GTranslationKeyframeSizes[MotionTransIT.Codec];
		appMemcpy(&Data[MotionTransKeyframesOffset], MotionTransIT.EncodedKeys.GetData(), KeySize * NumFrames);
	}
	// Regular rotation bundles.
	for (INT b = 0; b < NumRotBundles; b++)
	{
		const FIntermediateBundle& IB = RotBundles(b);
		const INT NT = IB.TrackIndices.Num();
		const INT KeySize = GRotationKeyframeSizes[IB.Codec];
		const INT HdrSize = GRotationHeaderSizes[IB.Codec];
		const INT TrackMapOff = RotBundleTrackMapOffsets(b);
		const INT HeaderOff = TrackMapOff + NT;
		const INT KeyframesOff = RotBundleKeyframesOffsets(b);

		FBundle* OB = (FBundle*)&Data[RotBundlesOffset + b * BundleSize];
		OB->Codec = IB.Codec;
		OB->NumTracks = (BYTE)NT;
		OB->NumFrames = (USHORT)IB.NumFrames;
		OB->TracksAndHeadersOffset = TrackMapOff;
		OB->KeyframesOffset = KeyframesOff;

		for (INT t = 0; t < NT; t++)
		{
			const FIntermediateRotationTrack& IT = RotInterm(IB.TrackIndices(t));
			Data[TrackMapOff + t] = IT.AnimTrackIndex;
			if (HdrSize > 0 && IT.Header.Num() == HdrSize)
			{
				appMemcpy(&Data[HeaderOff + t * HdrSize], IT.Header.GetData(), HdrSize);
			}
			// Interleave keys: decoder indexes KeySize * (t + NT * f)
			for (INT f = 0; f < IB.NumFrames; f++)
			{
				appMemcpy(
					&Data[KeyframesOff + KeySize * (t + NT * f)],
					&IT.EncodedKeys(KeySize * f),
					KeySize);
			}
		}
	}

	// Regular translation bundles.
	for (INT b = 0; b < NumTransBundles; b++)
	{
		const FIntermediateBundle& IB = TransBundles(b);
		const INT NT = IB.TrackIndices.Num();
		const INT KeySize = GTranslationKeyframeSizes[IB.Codec];
		const INT HdrSize = GTranslationHeaderSizes[IB.Codec];
		const INT TrackMapOff = TransBundleTrackMapOffsets(b);
		const INT HeaderOff = TrackMapOff + NT;
		const INT KeyframesOff = TransBundleKeyframesOffsets(b);

		FBundle* OB = (FBundle*)&Data[TransBundlesOffset + b * BundleSize];
		OB->Codec = IB.Codec;
		OB->NumTracks = (BYTE)NT;
		OB->NumFrames = (USHORT)IB.NumFrames;
		OB->TracksAndHeadersOffset = TrackMapOff;
		OB->KeyframesOffset = KeyframesOff;

		for (INT t = 0; t < NT; t++)
		{
			const FIntermediateTransScaleTrack& IT = TransInterm(IB.TrackIndices(t));
			Data[TrackMapOff + t] = IT.AnimTrackIndex;
			if (HdrSize > 0 && IT.Header.Num() == HdrSize)
			{
				appMemcpy(&Data[HeaderOff + t * HdrSize], IT.Header.GetData(), HdrSize);
			}
			for (INT f = 0; f < IB.NumFrames; f++)
			{
				appMemcpy(
					&Data[KeyframesOff + KeySize * (t + NT * f)],
					&IT.EncodedKeys(KeySize * f),
					KeySize);
			}
		}
	}

	// LinearOrigin/Span (BmGame.exe.c:11513269): relative to the first motion frame's transform.
	if (bHasMotionRot && bHasMotionTrans
		&& MotionRotIT.Samples.Num() > 0 && MotionTransIT.Samples.Num() > 0)
	{
		const FQuat   FirstQ = MotionRotIT.Samples(0);
		const FVector FirstT = MotionTransIT.Samples(0);
		const FVector LastT  = MotionTransIT.Samples(MotionTransIT.Samples.Num() - 1);
		const FVector Span   = LastT - FirstT;

		FVector Sum(0, 0, 0);
		for (INT f = 0; f < MotionTransIT.Samples.Num(); f++)
		{
			Sum += MotionTransIT.Samples(f);
		}
		const FVector Mean = Sum / (FLOAT)MotionTransIT.Samples.Num();

		const FQuat FirstQInv = FirstQ.Inverse();
		Seq->AnimZip_LinearOrigin = FirstQInv.RotateVector((Mean - 0.5f * Span) - FirstT);
		Seq->AnimZip_LinearSpan   = FirstQInv.RotateVector(Span);
	}
	else
	{
		Seq->AnimZip_LinearOrigin = FVector(0, 0, 0);
		Seq->AnimZip_LinearSpan   = FVector(0, 0, 0);
	}


	// Round-trip verification.
#if DO_CHECK
	for (INT i = 0; i < NRot; i++)
	{
		INT AnimTrack = IncludedRotTracks(i);
		const FRawAnimSequenceTrack& Track = Seq->RawAnimationData(AnimTrack);

		for (INT f = 0; f < NumFrames; f++)
		{
			FLOAT NormTime = (NumFrames > 1) ? (FLOAT)f / (NumFrames - 1) : 0.0f;
			NormTime = Clamp(NormTime, 0.0f, 1.0f - (FLOAT)SMALL_NUMBER);

			FBoneAtom Decoded;
			AnimZip_Sample_Track(Seq, AnimTrack, NormTime, &Decoded);

			INT KeyIdx = (Track.RotKeys.Num() > 1) ? Min(f, Track.RotKeys.Num() - 1) : 0;
			FQuat Expected = Track.RotKeys(KeyIdx);
			if (AnimTrack > 0)
			{
				Expected.W = -Expected.W;
			}
			Expected.Normalize();

			FQuat Got = Decoded.GetRotation();
			// Allow sign flip (Q and -Q are same rotation)
			FLOAT Dot = Abs(Expected.X * Got.X + Expected.Y * Got.Y + Expected.Z * Got.Z + Expected.W * Got.W);
			// Loose enough to cover both QuatMax_48 (dot ~= 1.0) and QuatMax_40 (theta up to a few degrees).
			checkf(Dot > 0.995f, TEXT("AnimZip round-trip rotation error too large for track %d frame %d (dot=%f)"), AnimTrack, f, Dot);
		}
	}

	for (INT i = 0; i < NTrans; i++)
	{
		INT AnimTrack = IncludedTransTracks(i);
		const FRawAnimSequenceTrack& Track = Seq->RawAnimationData(AnimTrack);
		for (INT f = 0; f < NumFrames; f++)
		{
			FLOAT NormTime = (NumFrames > 1) ? (FLOAT)f / (NumFrames - 1) : 0.0f;
			NormTime = Clamp(NormTime, 0.0f, 1.0f - (FLOAT)SMALL_NUMBER);

			FBoneAtom Decoded;
			AnimZip_Sample_Track(Seq, AnimTrack, NormTime, &Decoded);

			INT KeyIdx = (Track.PosKeys.Num() > 1) ? Min(f, Track.PosKeys.Num() - 1) : 0;
			FVector Expected = Track.PosKeys(KeyIdx);
			FVector Got = Decoded.GetTranslation();
			FLOAT Err = (Expected - Got).Size();
			// Covers both Float_96 (essentially lossless) and Interval_Fixed_48 (bounded by per-track range/65534).
			checkf(Err < 0.25f, TEXT("AnimZip round-trip translation error too large for track %d frame %d (err=%f)"), AnimTrack, f, Err);
		}
	}

	INT NQ48 = 0, NQ40 = 0;
	for (INT i = 0; i < RotInterm.Num(); i++)
	{
		if (RotInterm(i).Codec == AZRC_QuatMax_40) NQ40++; else NQ48++;
	}
	INT NF96 = 0, NI48 = 0;
	for (INT i = 0; i < TransInterm.Num(); i++)
	{
		if (TransInterm(i).Codec == AZTSC_NoScale_Interval_Fixed_48) NI48++; else NF96++;
	}
	debugf(TEXT("AnimZip_Compress: %s OK (rot: %d Q48 + %d Q40 in %d bundles; trans: %d F96 + %d I48 in %d bundles; %d frames, %d bytes)"),
		*Seq->SequenceName.ToString(), NQ48, NQ40, NumRotBundles, NF96, NI48, NumTransBundles, NumFrames, TotalSize);
#endif

	// Kept in the editor so PostEditChangeProperty can re-encode; the cooker strips it via StripData.
	Seq->CompressedTrackOffsets.Empty();
	Seq->CompressedByteStream.Empty();

	// Serialize() calls AnimationFormat_SetInterfaceLinks on the now-empty stream, so it needs a valid format.
	Seq->KeyEncodingFormat = AKF_ConstantKeyLerp;
	Seq->TranslationCompressionFormat = ACF_None;
	Seq->RotationCompressionFormat = ACF_None;
}

#endif

void UAnimSequence::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	//@compatibility:
	if( Ar.Ver() < VER_NATIVE_RAWANIMDATA_SERIALIZATION )
	{
		// Deprecated old serialization. Copy the script serialized array over to the new
		// property. Sadly there is no good way to change from script to native serialization
		// in a straightforward way without introducing a new property.
		RawAnimationData = RawAnimData_DEPRECATED;
		RawAnimData_DEPRECATED.Empty();
	}
	else
	{
		// NOTE: FRawAnimSequenceStruct uses BulkSerialize internally.
		check(RawAnimData_DEPRECATED.Num() == 0);
		Ar << RawAnimationData;
	}

	if ( Ar.IsLoading() )
	{
		// Serialize the compressed byte stream from the archive to the buffer.
		INT NumBytes;
		Ar << NumBytes;

		TArray<BYTE> SerializedData;
		SerializedData.Empty( NumBytes );
		SerializedData.Add( NumBytes );
		Ar.Serialize( SerializedData.GetData(), NumBytes );

		// Swap the buffer into the byte stream.
		FMemoryReader MemoryReader( SerializedData, TRUE );
		MemoryReader.SetByteSwapping( Ar.ForceByteSwapping() );

		// we must know the proper codecs to use
		AnimationFormat_SetInterfaceLinks(*this);

#if BATMAN
		if (Ar.IsBmCooked(TRUE))
		{
			Ar << AnimZip_Data;
		}
#endif

		// and then use the codecs to byte swap
		check( RotationCodec != NULL );
		((AnimationEncodingFormat*)RotationCodec)->ByteSwapIn(*this, MemoryReader, Ar.Ver());
	}
	else if( Ar.IsSaving() || Ar.IsCountingMemory() )
	{
		// Swap the byte stream into a buffer.
		TArray<BYTE> SerializedData;

		// we must know the proper codecs to use
		AnimationFormat_SetInterfaceLinks(*this);

		// and then use the codecs to byte swap
		check( RotationCodec != NULL );
		((AnimationEncodingFormat*)RotationCodec)->ByteSwapOut(*this, SerializedData, Ar.ForceByteSwapping());

		// Make sure the entire byte stream was serialized.
		check( CompressedByteStream.Num() == SerializedData.Num() );

		// Serialize the buffer to archive.
		INT Num = SerializedData.Num();
		Ar << Num;
		Ar.Serialize( SerializedData.GetData(), SerializedData.Num() );

		// Count compressed data.
		Ar.CountBytes( SerializedData.Num(), SerializedData.Num() );

#if BATMAN
		if (Ar.IsBmCooked(TRUE))
		{
			Ar << AnimZip_Data;
		}
#endif
	}
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UAnimSequence::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

	// if we aren't keeping any non-stripped platforms, we can toss the data
	if (!(PlatformsToKeep & ~UE3::PLATFORM_Stripped))
	{
		// Remove raw animation data.
		for( INT TrackIndex = 0 ; TrackIndex < RawAnimationData.Num() ; ++TrackIndex )
		{
			FRawAnimSequenceTrack& RawTrack = RawAnimationData(TrackIndex);
			RawTrack.PosKeys.Empty();
			RawTrack.RotKeys.Empty();
		}
		RawAnimationData.Empty();
	}
}

void UAnimSequence::PreSave()
{
	// UAnimSequence::CompressionScheme is editoronly, but animations could be compressed during cook
	// via PostLoad; so, clear the reference before saving in the cooker,
	if ( GIsCooking )
	{
		CompressionScheme = NULL;
	}
}

void UAnimSequence::PostLoad()
{
	UBOOL bMarkDirty = FALSE;
	Super::PostLoad();

#if BATMAN
	// AnimZip_Data is the playback path; ensure stale standard-compressed data isn't used.
	// RawAnimationData is kept in the editor so PostEditChangeProperty can re-encode.
	if (AnimZip_Data.Num())
	{
		CompressedTrackOffsets.Empty();
	}
#endif

#if !CONSOLE
	// If RAW animation data exists, and needs to be recompressed, do so.
	if( GIsEditor && GetLinkerVersion() < VER_FIXED_MALFORMED_RAW_ANIM_DATA && RawAnimationData.Num() > 0 )
	{
		// Recompress Raw Animation data w/ lossless compression.
		// If some keys have been removed, then recompress animsequence with its original compression algorithm
		CompressRawAnimData();
	}
#endif

	// Ensure notifies are sorted.
	SortNotifies();

	// Removing bad animations which contain no data from sets.
	if( GetLinkerVersion() < VER_REMOVE_BAD_ANIMSEQ 
		&& RawAnimationData.Num() == 0 
		&& CompressedTrackOffsets.Num() == 0 )
	{
#if !CONSOLE
		warnf( TEXT("Removing bad AnimSequence (%s) from %s."), *SequenceName.ToString(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
		UAnimSet* ParentSet = GetAnimSet();
		check( ParentSet );
		// Remove
		check( ParentSet->RemoveAnimSequenceFromAnimSet(this) );

		// Mark package dirty for resave
		bMarkDirty = TRUE;
#endif
	}
	// No animation data is found. Warn - this should check before we check CompressedTrackOffsets size
	// Otherwise, we'll see empty data set crashing game due to no CompressedTrackOffsets
	// You can't check RawAnimationData size since it gets removed during cooking
	else if ( NumFrames == 0 )
	{
		warnf( TEXT("No animation data exists for sequence %s (%s)"), *SequenceName.ToString(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
#if !CONSOLE
		if( GIsEditor )
		{
			warnf( TEXT("Removing bad AnimSequence (%s) from %s."), *SequenceName.ToString(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
			UAnimSet* ParentSet = GetAnimSet();
			ParentSet->RemoveAnimSequenceFromAnimSet(this);

			// Mark package dirty for resave
			bMarkDirty = TRUE;
		}
#endif
	}
	// Raw data exists, but missing compress animation data
	else if( CompressedTrackOffsets.Num() == 0 )
	{
#if CONSOLE
		// Never compress on consoles.
		appErrorf( TEXT("No animation compression exists for sequence %s (%s)"), *SequenceName.ToString(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
#else
#if BATMAN
#else
		warnf( TEXT("No animation compression exists for sequence %s (%s)"), *SequenceName.ToString(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
#endif
		// No animation compression, recompress using default settings.
		FAnimationUtils::CompressAnimSequence(this, NULL, FALSE, FALSE);
#endif // CONSOLE
	}

	static UBOOL ForcedRecompressionSetting = FAnimationUtils::GetForcedRecompressionSetting();

	// Recompress the animation if it was encoded with an old package set
	// or we are being forced to do so
	if (EncodingPkgVersion != CURRENT_ANIMATION_ENCODING_PACKAGE_VERSION ||
		ForcedRecompressionSetting)
	{
#if CONSOLE
		if (EncodingPkgVersion != CURRENT_ANIMATION_ENCODING_PACKAGE_VERSION)
		{
			// Never compress on consoles.
			appErrorf( TEXT("Animation compression method out of date for sequence %s"), *SequenceName.ToString() );
			CompressedTrackOffsets.Empty(0);
			CompressedByteStream.Empty(0);
		}
#else
		FAnimationUtils::CompressAnimSequence(this, NULL, TRUE, FALSE);
#endif // CONSOLE
	}

	// If we're in the game and compressed animation data exists, whack the raw data.
	if( GIsGame && !GIsEditor )
	{
		if( RawAnimationData.Num() > 0  && CompressedTrackOffsets.Num() > 0 )
		{
#if CONSOLE
			// Don't do this on consoles; raw animation data should have been stripped during cook!
			appErrorf( TEXT("Cooker did not strip raw animation from sequence %s"), *SequenceName.ToString() );
#else
			// Remove raw animation data.
			for ( INT TrackIndex = 0 ; TrackIndex < RawAnimationData.Num() ; ++TrackIndex )
			{
				FRawAnimSequenceTrack& RawTrack = RawAnimationData(TrackIndex);
				RawTrack.PosKeys.Empty();
				RawTrack.RotKeys.Empty();
			}
			
			RawAnimationData.Empty();
#endif // CONSOLE
		}
	}

#if !CONSOLE
	// swap out the deprecated revert to raw compression scheme with a least destructive compression scheme
	if (GIsEditor && GetAnimSet() && CompressionScheme && CompressionScheme->IsA(UDEPRECATED_AnimationCompressionAlgorithm_RevertToRaw::StaticClass()))
	{
		UAnimSet* AnimSet = GetAnimSet();
		warnf(TEXT("AnimSequence %s (%s) uses the deprecated revert to RAW compression scheme. Using least destructive compression scheme instead"), *GetName(), *AnimSet->GetFullName());
		USkeletalMesh* DefaultSkeletalMesh = LoadObject<USkeletalMesh>(NULL, *AnimSet->BestRatioSkelMeshName.ToString(), NULL, LOAD_None, NULL);
		UAnimationCompressionAlgorithm* NewAlgorithm = ConstructObject<UAnimationCompressionAlgorithm>( UAnimationCompressionAlgorithm_LeastDestructive::StaticClass() );
		NewAlgorithm->Reduce(this, DefaultSkeletalMesh, FALSE);
	}
#endif

	// setup the Codec interfaces
	AnimationFormat_SetInterfaceLinks(*this);

	// verify if it has valid curve keys. If not remove it. We don't have to save it. 
	for (INT I=0; I<CurveData.Num(); ++I)
	{
		// if not valid curve track remove this
		if (CurveData(I).IsValidCurveTrack() == FALSE)
		{
			// remove the item, no reason to have it
			CurveData.Remove(I);
			bMarkDirty = TRUE;
			--I;
		}
		else
		{
			bMarkDirty = CurveData(I).CompressCurveWeights() || bMarkDirty;
		}
	}

	if( bMarkDirty && (GIsRunning || GIsUCC) )
	{
		MarkPackageDirty();
	}

	if( GIsGame && !GIsEditor )
	{
		// this probably will not show newly created animations in PIE but will show them in the game once they have been saved off
		INC_DWORD_STAT_BY( STAT_AnimationMemory, GetResourceSize() );
	}
}

void UAnimSequence::BeginDestroy()
{
	Super::BeginDestroy();

	// clear any active codec links
	RotationCodec = NULL;
	TranslationCodec = NULL;

	if( GIsGame && !GIsEditor )
	{
		DEC_DWORD_STAT_BY( STAT_AnimationMemory, GetResourceSize() );
	}
}

void UAnimSequence::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(!IsTemplate())
	{
		// Make sure package is marked dirty when doing stuff like adding/removing notifies
		MarkPackageDirty();

#if BATMAN
		// Mirror BmGame.exe.c:6517748 - re-encode so compression setting edits take effect immediately.
		if (NumFrames > 0 && RawAnimationData.Num() > 0)
		{
			AnimZip_Compress(this);
		}
#endif
	}
}

// @todo DB: Optimize!
template<typename TimeArray>
static INT FindKeyIndex(FLOAT Time, const TimeArray& Times)
{
	INT FoundIndex = 0;
	for ( INT Index = 0 ; Index < Times.Num() ; ++Index )
	{
		const FLOAT KeyTime = Times(Index);
		if ( Time >= KeyTime )
		{
			FoundIndex = Index;
		}
		else
		{
			break;
		}
	}
	return FoundIndex;
}



/**
 * Populates the key reduced arrays from raw animation data.
 */
void UAnimSequence::SeparateRawDataToTracks(const TArray<FRawAnimSequenceTrack>& RawAnimData,
											FLOAT SequenceLength,
											TArray<FTranslationTrack>& OutTranslationData,
											TArray<FRotationTrack>& OutRotationData)
{
	// functionality moved to UAnimationCompressionAlgorithm for general use by Codecs
}

/**
* Interpolate curve weights of the Time in this sequence if curve data exists
*
* @param	Time			Time on track to interpolate to.
* @param	bLooping		TRUE if the animation is looping.
* @param	CurveKeys		Add the curve keys if exists
*/
void UAnimSequence::GetCurveData(FLOAT Time, UBOOL bLooping, FCurveKeyArray& CurveKeys) const
{
	// this code is almost replica of GetBoneAtom to get interpolated time to keep consistency between anim and curve
	if ( CurveData.Num() > 0 )
	{
		// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
		// by default, for looping animation, the last frame has a duration, and interpolates back to the first one.
		const INT NumKeys = bLooping ? NumFrames : NumFrames - 1;
		const FLOAT KeyPos = ((FLOAT)NumKeys * Time) / SequenceLength;

		// Find the integer part (ensuring within range) and that gives us the 'starting' key index.
		const INT KeyIndex1 = Clamp<INT>( appFloor(KeyPos), 0, NumFrames-1 );  // @todo should be changed to appTrunc

		// The alpha (fractional part) is then just the remainder.
		const FLOAT Alpha = (KeyPos - (FLOAT)KeyIndex1);

		INT KeyIndex2 = KeyIndex1 + 1;

		// If we have gone over the end, do different things in case of looping
		if( KeyIndex2 == NumFrames )
		{
			// If looping, interpolate between last and first frame
			if( bLooping )
			{
				KeyIndex2 = 0;
			}
			// If not looping - hold the last frame.
			else
			{
				KeyIndex2 = KeyIndex1;
			}
		}

		const INT PosKeyIndex1 = ::Min(KeyIndex1, NumFrames-1);
		const INT PosKeyIndex2 = ::Min(KeyIndex2, NumFrames-1);

		INT StartIdx=CurveKeys.Num();
		INT EndIdx=StartIdx + CurveData.Num();

		CurveKeys.AddZeroed(CurveData.Num());

		// go through every curve
		// current I don't cull here - even when weight is 0, it all adds up.
		// the reason is that I purge after collecting all to keep the consistency 
		for (INT I=StartIdx; I<EndIdx; ++I)
		{
			if ( NumFrames == CurveData(I-StartIdx).CurveWeights.Num() )
			{
				CurveKeys(I).CurveName = CurveData(I-StartIdx).CurveName;
				CurveKeys(I).Weight = CurveData(I-StartIdx).CurveWeights(PosKeyIndex1) + (CurveData(I-StartIdx).CurveWeights(PosKeyIndex2)-CurveData(I-StartIdx).CurveWeights(PosKeyIndex1))*(Alpha);
			}
			// if only 1 key, that means there is only one value
			else if ( 1 == CurveData(I-StartIdx).CurveWeights.Num() )
			{
				CurveKeys(I).CurveName = CurveData(I-StartIdx).CurveName;
				CurveKeys(I).Weight = CurveData(I-StartIdx).CurveWeights(0);
			}
			else
			{
				checkMsg( 1, *FString::Printf(TEXT("Curve Data is corrupted : [Sequence (%s): Curve Name (%s)]"), *SequenceName.GetNameString(), CurveData(I-StartIdx).CurveName) );
			}
//			debugf(TEXT("SequenceName: %s Weight is (%s: %0.5f) for Time %0.2f"), *SequenceName.GetNameString(), *CurveKeys(I).CurveName.GetNameString(), CurveKeys(I).Weight, Time);
		}
	}
}

/**
 * Interpolate keyframes in this sequence to find the bone transform (relative to parent).
 * 
 * @param	OutAtom			[out] Output bone transform.
 * @param	TrackIndex		Index of track to interpolate.
 * @param	Time			Time on track to interpolate to.
 * @param	bLooping		TRUE if the animation is looping.
 * @param	bUseRawData		If TRUE, use raw animation data instead of compressed data.
 * @param	CurveKeys		GetCurveKeys if exists
 */
void UAnimSequence::GetBoneAtom(FBoneAtom& OutAtom, INT TrackIndex, FLOAT Time, UBOOL bLooping, UBOOL bUseRawData, FCurveKeyArray* CurveKeys) const
{
	// If the caller didn't request that raw animation data be used . . .
	if ( !bUseRawData )
	{
#if BATMAN
		if ( AnimZip_Data.Num() > 0 )
		{
			FLOAT NormTime = (ClippedLength > 0.0f)
				? Clamp((Time - ClippedStart) / ClippedLength, 0.0f, 1.0f) : 0.0f;
			AnimZip_Sample_Track(this, TrackIndex, NormTime, &OutAtom);
			if ( CurveKeys != NULL && CurveData.Num() > 0 )
			{
				GetCurveData(Time, bLooping, *CurveKeys);
			}
			return;
		}
#endif
		if ( CompressedTrackOffsets.Num() > 0 )
		{
			AnimationFormat_GetBoneAtom( OutAtom, *this, TrackIndex, Time, bLooping );
			if ( CurveKeys != NULL && CurveData.Num() >  0 )
			{
				GetCurveData(Time, bLooping, *CurveKeys);
			}

			return;
		}
	}

	//const FScopedTimer Timer( TEXT("Raw") );
	OutAtom.SetScale(1.f);

	// Bail out if the animation data doesn't exists (e.g. was stripped by the cooker).
	if ( RawAnimationData.Num() == 0 )
	{
		debugf( NAME_DevAnim, TEXT("UAnimSequence::GetBoneAtom : No anim data in AnimSequence!") );
		OutAtom.SetIdentity();
		return;
	}

	const FRawAnimSequenceTrack& RawTrack = RawAnimationData(TrackIndex);

	// Bail out (with rather wacky data) if data is empty for some reason.
	if( RawTrack.PosKeys.Num() == 0 || RawTrack.RotKeys.Num() == 0 )
	{
		debugf( NAME_DevAnim, TEXT("UAnimSequence::GetBoneAtom : No anim data in AnimSequence!") );
		OutAtom.SetIdentity();
		return;
	}

   	// Check for 1-frame, before-first-frame and after-last-frame cases.
	if( Time <= 0.f || NumFrames == 1 )
	{
		OutAtom.SetTranslation(RawTrack.PosKeys(0));
		OutAtom.SetRotation(RawTrack.RotKeys(0));

		if( CurveKeys != NULL && CurveData.Num() >  0 )
		{
			GetCurveData(0.0f, FALSE, *CurveKeys);
		}
		return;
	}

	const INT LastIndex		= NumFrames - 1;
	const INT LastPosIndex	= ::Min(LastIndex, RawTrack.PosKeys.Num()-1);
	const INT LastRotIndex	= ::Min(LastIndex, RawTrack.RotKeys.Num()-1);
	if( Time >= SequenceLength )
	{
		// If we're not looping, key n-1 is the final key.
		// If we're looping, key 0 is the final key.
		OutAtom.SetTranslation( RawTrack.PosKeys( bLooping ? 0 : LastPosIndex ) );
		OutAtom.SetRotation( RawTrack.RotKeys( bLooping ? 0 : LastRotIndex ) );
		return;
	}

	// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
	// by default, for looping animation, the last frame has a duration, and interpolates back to the first one.
	const INT NumKeys = bLooping ? NumFrames : NumFrames - 1;
	const FLOAT KeyPos = ((FLOAT)NumKeys * Time) / SequenceLength;

// 	debugf(TEXT(" *  *  *  GetBoneAtom. Time: %f, KeyPos: %f"), Time, KeyPos);

	// Find the integer part (ensuring within range) and that gives us the 'starting' key index.
	const INT KeyIndex1 = Clamp<INT>( appFloor(KeyPos), 0, NumFrames-1 );  // @todo should be changed to appTrunc

	// The alpha (fractional part) is then just the remainder.
	const FLOAT Alpha = KeyPos - (FLOAT)KeyIndex1;

	INT KeyIndex2 = KeyIndex1 + 1;

	// If we have gone over the end, do different things in case of looping
	if( KeyIndex2 == NumFrames )
	{
		// If looping, interpolate between last and first frame
		if( bLooping )
		{
			KeyIndex2 = 0;
		}
		// If not looping - hold the last frame.
		else
		{
			KeyIndex2 = KeyIndex1;
		}
	}

	const INT PosKeyIndex1 = ::Min(KeyIndex1, RawTrack.PosKeys.Num()-1);
	const INT RotKeyIndex1 = ::Min(KeyIndex1, RawTrack.RotKeys.Num()-1);
	const INT PosKeyIndex2 = ::Min(KeyIndex2, RawTrack.PosKeys.Num()-1);
	const INT RotKeyIndex2 = ::Min(KeyIndex2, RawTrack.RotKeys.Num()-1);

// 	debugf(TEXT(" *  *  *  Position. PosKeyIndex1: %3d, PosKeyIndex2: %3d, Alpha: %f"), PosKeyIndex1, PosKeyIndex2, Alpha);
// 	debugf(TEXT(" *  *  *  Rotation. RotKeyIndex1: %3d, RotKeyIndex2: %3d, Alpha: %f"), RotKeyIndex1, RotKeyIndex2, Alpha);
	OutAtom.SetTranslation( Lerp(RawTrack.PosKeys(PosKeyIndex1), RawTrack.PosKeys(PosKeyIndex2), Alpha) );

#if !USE_SLERP
	// Fast linear quaternion interpolation.
	// To ensure the 'shortest route', we make sure the dot product between the two keys is positive.
	if( (RawTrack.RotKeys(RotKeyIndex1) | RawTrack.RotKeys(RotKeyIndex2)) < 0.f )
	{
		// To clarify the code here: a slight optimization of inverting the parametric variable as opposed to the quaternion.
		OutAtom.SetRotation(  (RawTrack.RotKeys(RotKeyIndex1) * (1.f-Alpha)) + (RawTrack.RotKeys(RotKeyIndex2) * -Alpha) );
	}
	else
	{
		OutAtom.SetRotation(  (RawTrack.RotKeys(RotKeyIndex1) * (1.f-Alpha)) + (RawTrack.RotKeys(RotKeyIndex2) * Alpha) );
	}
#else
	OutAtom.SetRotation( SlerpQuat( RawTrack.RotKeys(RotKeyIndex1), RawTrack.RotKeys(RotKeyIndex2), Alpha ) );
#endif
	OutAtom.NormalizeRotation();
	
	// get curve keys if any
	if ( CurveKeys != NULL && CurveData.Num() >  0 )
	{
		GetCurveData(Time, bLooping, *CurveKeys);
	}
}

IMPLEMENT_COMPARE_CONSTREF( FAnimNotifyEvent, UnSkeletalAnim,
{
	if		(A.Time > B.Time)	return 1;
	else if	(A.Time < B.Time)	return -1;
	else						return 0;
} 
)

/**
 * Sort the Notifies array by time, earliest first.
 */
void UAnimSequence::SortNotifies()
{
	Sort<USE_COMPARE_CONSTREF(FAnimNotifyEvent,UnSkeletalAnim)>(&Notifies(0),Notifies.Num());
}

/**
 * @return		A reference to the AnimSet this sequence belongs to.
 */
UAnimSet* UAnimSequence::GetAnimSet() const
{
	return CastChecked<UAnimSet>( GetOuter() );
}

/** Utility function to crop data from a RawAnimSequenceTrack */
static INT CropRawTrack(FRawAnimSequenceTrack& RawTrack, INT StartKey, INT NumKeys, INT TotalNumOfFrames)
{
	check(RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == TotalNumOfFrames);
	check(RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == TotalNumOfFrames);

	if( RawTrack.PosKeys.Num() > 1 )
	{
		RawTrack.PosKeys.Remove(StartKey, NumKeys);
		check(RawTrack.PosKeys.Num() > 0);
		RawTrack.PosKeys.Shrink();
	}

	if( RawTrack.RotKeys.Num() > 1 )
	{
		RawTrack.RotKeys.Remove(StartKey, NumKeys);
		check(RawTrack.RotKeys.Num() > 0);
		RawTrack.RotKeys.Shrink();
	}

	// Update NumFrames below to reflect actual number of keys.
	return Max<INT>( RawTrack.PosKeys.Num(), RawTrack.RotKeys.Num() );
}

/**
 * Crops the raw anim data either from Start to CurrentTime or CurrentTime to End depending on 
 * value of bFromStart.  Can't be called against cooked data.
 * @Note: Animation must be recompressed after this. As this only affects Raw data.
 *
 * @param	CurrentTime		marker for cropping (either beginning or end)
 * @param	bFromStart		whether marker is begin or end marker
 * @return					TRUE if the operation was successful.
 */
UBOOL UAnimSequence::CropRawAnimData( FLOAT CurrentTime, UBOOL bFromStart )
{
	if (GIsCooking)
	{
		if (HasAnyFlags(RF_MarkedByCooker))
		{
			return FALSE;
		}
	}
	else
	{
		// Can't crop cooked animations.
		const UPackage* Package = GetOutermost();
		if( Package->PackageFlags & PKG_Cooked )
		{
			return FALSE;
		}
	}

	// Length of one frame.
	FLOAT const FrameTime = SequenceLength / ((FLOAT)NumFrames);
	// Save Total Number of Frames before crop
	INT TotalNumOfFrames = NumFrames;

	// if current frame is 1, do not try crop. There is nothing to crop
	if ( NumFrames <= 1 )
	{
		return FALSE;
	}
	
	// If you're end or beginning, you can't cut all nor nothing. 
	// Avoiding ambiguous situation what exactly we would like to cut 
	// Below it clamps range to 1, TotalNumOfFrames-1
	// causing if you were in below position, it will still crop 1 frame. 
	// To be clearer, it seems better if we reject those inputs. 
	// If you're a bit before/after, we assume that you'd like to crop
	if ( CurrentTime == 0.f || CurrentTime == SequenceLength )
	{
		return FALSE;
	}

	// Find the right key to cut at.
	// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
	// The reason I'm changing to TotalNumOfFrames is CT/SL = KeyIndexWithFraction/TotalNumOfFrames
	// To play TotalNumOfFrames, it takes SequenceLength. Each key will take SequenceLength/TotalNumOfFrames
	FLOAT const KeyIndexWithFraction = (CurrentTime * (FLOAT)(TotalNumOfFrames)) / SequenceLength;
	INT KeyIndex = bFromStart ? appFloor(KeyIndexWithFraction) : appCeil(KeyIndexWithFraction);
	// Ensure KeyIndex is in range.
	KeyIndex = Clamp<INT>(KeyIndex, 1, TotalNumOfFrames-1); 
	// determine which keys need to be removed.
	INT const StartKey = bFromStart ? 0 : KeyIndex;
	INT const NumKeys = bFromStart ? KeyIndex : TotalNumOfFrames - KeyIndex ;

	// Recalculate NumFrames
	NumFrames = TotalNumOfFrames - NumKeys;

	debugf(TEXT("UAnimSequence::CropRawAnimData %s - CurrentTime: %f, bFromStart: %d, TotalNumOfFrames: %d, KeyIndex: %d, StartKey: %d, NumKeys: %d"), *SequenceName.ToString(), CurrentTime, bFromStart, TotalNumOfFrames, KeyIndex, StartKey, NumKeys);

	// Iterate over tracks removing keys from each one.
	for(INT i=0; i<RawAnimationData.Num(); i++)
	{
		CropRawTrack(RawAnimationData(i), StartKey, NumKeys, TotalNumOfFrames);
	}

	// Double check that everything is fine
	for(INT i=0; i<RawAnimationData.Num(); i++)
	{
		FRawAnimSequenceTrack& RawTrack = RawAnimationData(i);
		check(RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == NumFrames);
		check(RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == NumFrames);
	}

	// Crop curve data 
	for( INT CurveTrackIdx = 0; CurveTrackIdx < CurveData.Num(); ++CurveTrackIdx )
	{
		if( CurveData(CurveTrackIdx).CurveWeights.Num() > 1 )
		{
			CurveData(CurveTrackIdx).CurveWeights.Remove(StartKey, NumKeys);
			CurveData(CurveTrackIdx).CompressCurveWeights();

			check(CurveData(CurveTrackIdx).CurveWeights.Num() == 1 || CurveData(CurveTrackIdx).CurveWeights.Num() == NumFrames );
		}
	}

	// Update sequence length to match new number of frames.
	SequenceLength = (FLOAT)NumFrames * FrameTime;

	debugf(TEXT("\tSequenceLength: %f, NumFrames: %d"), SequenceLength, NumFrames);

	MarkPackageDirty();
	return TRUE;
}

/** Utility function to losslessly compress a FRawAnimSequenceTrack */
UBOOL UAnimSequence::CompressRawAnimSequenceTrack(FRawAnimSequenceTrack& RawTrack, FLOAT MaxPosDiff, FLOAT MaxAngleDiff)
{
	UBOOL bRemovedKeys = FALSE;

	// First part is to make sure we have valid input
	UBOOL const bPosTrackIsValid = (RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == NumFrames);
	if( !bPosTrackIsValid )
	{
#if BATMAN
		// TEMP
#else
		warnf(TEXT("Found non valid position track for %s, %d frames, instead of %d. Chopping!"), *SequenceName.ToString(), RawTrack.PosKeys.Num(), NumFrames);
#endif
		bRemovedKeys = TRUE;
		RawTrack.PosKeys.Remove(1, RawTrack.PosKeys.Num()- 1);
		RawTrack.PosKeys.Shrink();
		check( RawTrack.PosKeys.Num() == 1);
	}

	UBOOL const bRotTrackIsValid = (RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == NumFrames);
	if( !bRotTrackIsValid )
	{
#if BATMAN
		// TEMP
#else
		warnf(TEXT("Found non valid rotation track for %s, %d frames, instead of %d. Chopping!"), *SequenceName.ToString(), RawTrack.RotKeys.Num(), NumFrames);
#endif
		bRemovedKeys = TRUE;
		RawTrack.RotKeys.Remove(1, RawTrack.RotKeys.Num()- 1);
		RawTrack.RotKeys.Shrink();
		check( RawTrack.RotKeys.Num() == 1);
	}

	// Second part is actual compression.

	// Check variation of position keys
	if( (RawTrack.PosKeys.Num() > 1) && (MaxPosDiff >= 0.0f) )
	{
		FVector FirstPos = RawTrack.PosKeys(0);
		UBOOL bFramesIdentical = TRUE;
		for(INT j=1; j<RawTrack.PosKeys.Num() && bFramesIdentical; j++)
		{
			if( (FirstPos - RawTrack.PosKeys(j)).Size() > MaxPosDiff )
			{
				bFramesIdentical = FALSE;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			bRemovedKeys = TRUE;
			RawTrack.PosKeys.Remove(1, RawTrack.PosKeys.Num()- 1);
			RawTrack.PosKeys.Shrink();
			check( RawTrack.PosKeys.Num() == 1);
		}
	}

	// Check variation of rotational keys
	if( (RawTrack.RotKeys.Num() > 1) && (MaxAngleDiff >= 0.0f) )
	{
		FQuat FirstRot = RawTrack.RotKeys(0);
		UBOOL bFramesIdentical = TRUE;
		for(INT j=1; j<RawTrack.RotKeys.Num() && bFramesIdentical; j++)
		{
			if( FQuatError(FirstRot, RawTrack.RotKeys(j)) > MaxAngleDiff )
			{
				bFramesIdentical = FALSE;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			bRemovedKeys = TRUE;
			RawTrack.RotKeys.Remove(1, RawTrack.RotKeys.Num()- 1);
			RawTrack.RotKeys.Shrink();
			check( RawTrack.RotKeys.Num() == 1);
		}			
	}

	return bRemovedKeys;
}

/**
 * Removes trivial frames -- frames of tracks when position or orientation is constant
 * over the entire animation -- from the raw animation data.  If both position and rotation
 * go down to a single frame, the time is stripped out as well.
 */
UBOOL UAnimSequence::CompressRawAnimData(float MaxPosDiff, float MaxAngleDiff)
{
	// Only bother doing anything if we have some keys!
	if( NumFrames == 1 )
	{
		return FALSE;
	}

	UBOOL bRemovedKeys = FALSE;
	// Raw animation data
	for(INT i=0; i<RawAnimationData.Num(); i++)
	{
		bRemovedKeys = CompressRawAnimSequenceTrack( RawAnimationData(i), MaxPosDiff, MaxAngleDiff ) || bRemovedKeys;
	}

	return bRemovedKeys;
}

/**
 * Removes trivial frames -- frames of tracks when position or orientation is constant
 * over the entire animation -- from the raw animation data.  If both position and rotation
 * go down to a single frame, the time is stripped out as well.
 */
UBOOL UAnimSequence::CompressRawAnimData()
{
	const FLOAT MaxPosDiff = 0.0001f;
	const FLOAT MaxAngleDiff = 0.0003f;
	return CompressRawAnimData(MaxPosDiff, MaxAngleDiff);
}

/** Clears any data in the AnimSequence, so it can be recycled when importing a new animation with same name over it. */
void UAnimSequence::RecycleAnimSequence()
{
	// Clear RawAnimData
	RawAnimationData.Empty();
}

/** 
 * Utility function to copy all UAnimSequence properties from Source to Destination.
 * Does not copy however RawAnimData and CompressedAnimData.
 */
UBOOL UAnimSequence::CopyAnimSequenceProperties(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq, UBOOL bSkipCopyingNotifies)
{
	// Copy parameters
	DestAnimSeq->SequenceName				= SourceAnimSeq->SequenceName;
	DestAnimSeq->SequenceLength				= SourceAnimSeq->SequenceLength;
	DestAnimSeq->NumFrames					= SourceAnimSeq->NumFrames;
	DestAnimSeq->RateScale					= SourceAnimSeq->RateScale;
	DestAnimSeq->bDoNotOverrideCompression	= SourceAnimSeq->bDoNotOverrideCompression;

	// Copy Compression Settings
	DestAnimSeq->CompressionScheme				= SourceAnimSeq->CompressionScheme;
	DestAnimSeq->TranslationCompressionFormat	= SourceAnimSeq->TranslationCompressionFormat;
	DestAnimSeq->RotationCompressionFormat		= SourceAnimSeq->RotationCompressionFormat;

	if( !bSkipCopyingNotifies )
	{
		CopyNotifies(SourceAnimSeq, DestAnimSeq);
	}

	DestAnimSeq->MarkPackageDirty();

	// Copy Curve Data
	DestAnimSeq->CurveData = SourceAnimSeq->CurveData;

	return TRUE;
}

/**
 * Copy AnimNotifies from one UAnimSequence to another.
 */
UBOOL UAnimSequence::CopyNotifies(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq)
{
	// Abort if source == destination.
	if( SourceAnimSeq == DestAnimSeq )
	{
		return TRUE;
	}

	// If the destination sequence is shorter than the source sequence, we'll be dropping notifies that
	// occur at later times than the dest sequence is long.  Give the user a chance to abort if we
	// find any notifies that won't be copied over.
	if( DestAnimSeq->SequenceLength < SourceAnimSeq->SequenceLength )
	{
		for(INT NotifyIndex=0; NotifyIndex<SourceAnimSeq->Notifies.Num(); ++NotifyIndex)
		{
			// If a notify is found which occurs off the end of the destination sequence, prompt the user to continue.
			const FAnimNotifyEvent& SrcNotifyEvent = SourceAnimSeq->Notifies(NotifyIndex);
			if( SrcNotifyEvent.Time > DestAnimSeq->SequenceLength )
			{
				const UBOOL bProceed = appMsgf( AMT_YesNo, *LocalizeUnrealEd("SomeNotifiesWillNotBeCopiedQ") );
				if( !bProceed )
				{
					return FALSE;
				}
				else
				{
					break;
				}
			}
		}
	}

	// If the destination sequence contains any notifies, ask the user if they'd like
	// to delete the existing notifies before copying over from the source sequence.
	if( DestAnimSeq->Notifies.Num() > 0 )
	{
		const UBOOL bDeleteExistingNotifies = appMsgf( AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("DestSeqAlreadyContainsNotifiesMergeQ"), DestAnimSeq->Notifies.Num()) );
		if( bDeleteExistingNotifies )
		{
			DestAnimSeq->Notifies.Empty();
			DestAnimSeq->MarkPackageDirty();
		}
	}

	// Do the copy.
	TArray<INT> NewNotifyIndices;
	INT NumNotifiesThatWereNotCopied = 0;

	for(INT NotifyIndex=0; NotifyIndex<SourceAnimSeq->Notifies.Num(); ++NotifyIndex)
	{
		const FAnimNotifyEvent& SrcNotifyEvent = SourceAnimSeq->Notifies(NotifyIndex);

		// Skip notifies which occur at times later than the destination sequence is long.
		if( SrcNotifyEvent.Time > DestAnimSeq->SequenceLength )
		{
			continue;
		}

		// Do a linear-search through existing notifies to determine where
		// to insert the new notify.
		INT NewNotifyIndex = 0;
		while( NewNotifyIndex < DestAnimSeq->Notifies.Num()
			&& DestAnimSeq->Notifies(NewNotifyIndex).Time <= SrcNotifyEvent.Time )
		{
			++NewNotifyIndex;
		}

		// Track the location of the new notify.
		NewNotifyIndices.AddItem(NewNotifyIndex);

		// Create a new empty on in the array.
		DestAnimSeq->Notifies.InsertZeroed(NewNotifyIndex);

		// Copy time.
		DestAnimSeq->Notifies(NewNotifyIndex).Time = SrcNotifyEvent.Time;

		// Copy the notify itself, and point the new one at it.
		if( SrcNotifyEvent.Notify )
		{
			FObjectDuplicationParameters DupParams( SrcNotifyEvent.Notify, DestAnimSeq );
			DestAnimSeq->Notifies(NewNotifyIndex).Notify = CastChecked<UAnimNotify>( UObject::StaticDuplicateObjectEx(DupParams) );
		}
		else
		{
			DestAnimSeq->Notifies(NewNotifyIndex).Notify = NULL;
		}

		// Make sure editor knows we've changed something.
		DestAnimSeq->MarkPackageDirty();
	}

	// Inform the user if some notifies weren't copied.
	if( SourceAnimSeq->Notifies.Num() > NewNotifyIndices.Num() )
	{
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("SomeNotifiesWereNotCopiedF"), SourceAnimSeq->Notifies.Num() - NewNotifyIndices.Num()) );
	}

	return TRUE;
}

FLOAT UAnimSequence::GetNotifyTimeByClass( UClass* NotifyClass, FLOAT PlayRate, FLOAT StartPosition, UAnimNotify** out_Notify, FLOAT* out_Duration )
{
	if( PlayRate <= 0.f )
	{
		PlayRate = 1.f;
	}
	for( INT i = 0; i < Notifies.Num(); i++ )
	{
		UClass* C = (Notifies(i).Notify != NULL) ? Notifies(i).Notify->GetClass() : NULL;
		FLOAT NotifyTime = Notifies(i).Time / PlayRate;
		if( C != NULL && C->IsChildOf( NotifyClass ) && NotifyTime > StartPosition )
		{
			if( out_Notify != NULL )
			{
				*out_Notify = Notifies(i).Notify;
			}
			if( out_Duration != NULL )
			{
				*out_Duration = 0.f;
			}

			return NotifyTime;
		}
	}

	return -1.f;
}

/*-----------------------------------------------------------------------------
	UAnimSet
-----------------------------------------------------------------------------*/

void FAnimSetMeshLinkup::BuildLinkup(USkeletalMesh* InSkelMesh, UAnimSet* InAnimSet)
{
	INT const NumBones = InSkelMesh->RefSkeleton.Num();

	// Bone to Track mapping.
	BoneToTrackTable.Empty(NumBones);
	BoneToTrackTable.Add(NumBones);

	// For each bone in skeletal mesh, find which track to pull from in the AnimSet.
	for(INT i=0; i<NumBones; i++)
	{
		FName const BoneName = InSkelMesh->RefSkeleton(i).Name;

		// FindTrackWithName will return INDEX_NONE if no track exists.
		BoneToTrackTable(i) = InAnimSet->FindTrackWithName(BoneName);
	}

	{
		INT const NumTracks = InAnimSet->TrackBoneNames.Num();
		TrackToBoneTable.Empty(NumTracks);
		TrackToBoneTable.Add(NumTracks);
		for (INT t = 0; t < NumTracks; t++)
		{
			TrackToBoneTable(t) = INDEX_NONE;
		}
		for (INT b = 0; b < NumBones; b++)
		{
			INT t = BoneToTrackTable(b);
			if (t != INDEX_NONE && t < NumTracks)
			{
				TrackToBoneTable(t) = b;
			}
		}
	}

#if !FINAL_RELEASE
	TArray<UBOOL> TrackUsed;
	TrackUsed.AddZeroed(InAnimSet->TrackBoneNames.Num());
	const INT AnimLinkupIndex = InAnimSet->GetMeshLinkupIndex( InSkelMesh );
	const FAnimSetMeshLinkup& AnimLinkup = InAnimSet->LinkupCache( AnimLinkupIndex );
	for(INT BoneIndex=0; BoneIndex<NumBones; BoneIndex++)
	{
		const INT TrackIndex = AnimLinkup.BoneToTrackTable(BoneIndex);

		if( TrackIndex == INDEX_NONE )
		{
			continue;
		}

		if( TrackUsed(TrackIndex) )
		{
			warnf(TEXT("%s has multiple bones sharing the same track index!!!"), *InAnimSet->GetFullName());	
			for(INT DupeBoneIndex=0; DupeBoneIndex<NumBones; DupeBoneIndex++)
			{
				const INT DupeTrackIndex = AnimLinkup.BoneToTrackTable(DupeBoneIndex);
				if( DupeTrackIndex == TrackIndex )
				{
					warnf(TEXT(" BoneIndex: %i, BoneName: %s, TrackIndex: %i, TrackBoneName: %s"), DupeBoneIndex, *InSkelMesh->RefSkeleton(DupeBoneIndex).Name.ToString(), DupeTrackIndex, *InAnimSet->TrackBoneNames(DupeTrackIndex).ToString());	
				}
			}
		}

		TrackUsed(TrackIndex) = TRUE;
	}
#endif
}


// Global flag to trace animation usage
// Currently gets activated with only command "-TRACEANIMUSAGE"
extern UBOOL GShouldTraceAnimationUsage;

void UAnimSet::PostLoad()
{
	Super::PostLoad();

	// Populate sequence cache.
	for( INT i=0; i<Sequences.Num(); i++ )
	{
		UAnimSequence* AnimSequence = Sequences(i);
		if( AnimSequence )
		{
			SequenceCache.Set( AnimSequence->SequenceName, i ); 
		}
	}
	
	// Make sure that AnimSets (and sequences) within level packages are not marked as standalone.
	if(GetOutermost()->ContainsMap() && HasAnyFlags(RF_Standalone))
	{
		ClearFlags(RF_Standalone);

		for(INT i=0; i<Sequences.Num(); i++)
		{
			UAnimSequence* Seq = Sequences(i);
			if(Seq)
			{
				Seq->ClearFlags(RF_Standalone);
			}
		}
	}

	// BM2: Joker_Thug_3_Head doesn't exist in retail
	if( PreviewExtraSkelMesh1Name == FName(TEXT("Joker_Thugs.Mesh.Joker_Thug_3_Head")) )
	{
		PreviewExtraSkelMesh1Name = FName(TEXT("Joker_Thugs.Mesh.Joker_Thug_4_Head"));
	}

	// If we're tracing animation usage, start
	if ( GShouldTraceAnimationUsage )
	{
		TraceAnimationUsage();
	}
}	

void UAnimSet::BeginDestroy()
{
	if ( GShouldTraceAnimationUsage )
	{
		RecordAnimationUsage();
	}

	Super::BeginDestroy();
}

void UAnimSet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// if the bAnimRotationOnly was set to false, re-compress the sequences to ensure that translations are added back into the animations
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged && (PropertyThatChanged->GetName() == TEXT("bAnimRotationOnly")) && !bAnimRotationOnly)
	{
		for(INT i=0; i<Sequences.Num(); i++)
		{
			UAnimSequence* Seq = Sequences(i);
			if(Seq)
			{
				FAnimationUtils::CompressAnimSequence(Seq, NULL, FALSE, FALSE);
			}
		}
	}
}

/**
 * See if we can play sequences from this AnimSet on the provided SkeletalMesh.
 * Returns true if there is a bone in SkelMesh for every track in the AnimSet,
 * or there is a track of animation for every bone of the SkelMesh.
 * 
 * @param	SkelMesh	SkeletalMesh to compare the AnimSet against.
 * @return				TRUE if animation set can play on supplied SkeletalMesh, FALSE if not.
 */
UBOOL UAnimSet::CanPlayOnSkeletalMesh(USkeletalMesh* SkelMesh) const
{
	// Temporarily allow any animation to play on any AnimSet. 
	// We need a looser metric for matching animation to skeletons. Some 'overlap bone count'?
#if 0
	// This is broken and needs to be looked into.
	// we require at least 10% of tracks matched by skeletal mesh.
	return GetSkeletalMeshMatchRatio(SkelMesh) > 0.1f;
#else
	return TRUE;
#endif
}

FLOAT UAnimSet::GetSkeletalMeshMatchRatio(USkeletalMesh* SkelMesh) const
{
	// First see if there is a bone for all tracks
	INT TracksMatched = 0;
	for(INT i=0; i<TrackBoneNames.Num() ; i++)
	{
		const INT BoneIndex = SkelMesh->MatchRefBone( TrackBoneNames(i) );
		if( BoneIndex != INDEX_NONE )
		{
			++TracksMatched;
		}
	}

	// If we can't match any bones, then this is definitely not compatible.
	if( TrackBoneNames.Num() == 0 || TracksMatched == 0 )
	{
		return 0.f;
	}

	// return how many of the animation tracks were matched by that mesh
	return (FLOAT)TracksMatched / FLOAT(TrackBoneNames.Num());
}

/**
 * Returns the AnimSequence with the specified name in this set.
 * 
 * @param		SequenceName	Name of sequence to find.
 * @return						Pointer to AnimSequence with desired name, or NULL if sequence was not found.
 */
UAnimSequence* UAnimSet::FindAnimSequence(FName SequenceName)
{
	UAnimSequence* AnimSequence = NULL;

	if( SequenceName != NAME_None )
	{
		const INT* IndexPtr = SequenceCache.Find( SequenceName );
		if( IndexPtr )
		{
			// Check for mismatch and clear entire cache if that happens.
			AnimSequence = Sequences(Min(*IndexPtr,Sequences.Num()-1));
			if( AnimSequence->SequenceName != SequenceName )
			{
				check(GIsEditor);
				AnimSequence = NULL;
				SequenceCache.Empty();
			}
		}		
		// Modifications in the editor might have invalidated the cache or there are new entries.
		if( GIsEditor && AnimSequence == NULL )
		{
			for(INT i=0; i<Sequences.Num(); i++)
			{
				if( Sequences(i)->SequenceName == SequenceName )
				{				
					AnimSequence = Sequences(i);
					// Populate cache on demand with new entries.
					SequenceCache.Set( SequenceName, i );
					break;
				}
			}
		}
	}

	return AnimSequence;
}

/**
 * Find a mesh linkup table (mapping of sequence tracks to bone indices) for a particular SkeletalMesh
 * If one does not already exist, create it now.
 *
 * @param InSkelMesh SkeletalMesh to look for linkup with.
 *
 * @return Index of Linkup between mesh and animation set.
 */
INT UAnimSet::GetMeshLinkupIndex(USkeletalMesh* InSkelMesh)
{
	// First, see if we have a cached link-up between this animation and the given skeletal mesh.
	check(InSkelMesh);

	// Get SkeletalMesh path name
	FName SkelMeshName = FName( *InSkelMesh->GetPathName() );

	// See if we have already cached this Skeletal Mesh.
	const INT* IndexPtr = SkelMesh2LinkupCache.Find( SkelMeshName );

	// If not found, create a new entry
	if( IndexPtr == NULL )
	{
		// No linkup found - so create one here and add to cache.
		const INT NewLinkupIndex = LinkupCache.AddZeroed();

		// Add it to our cache
		SkelMesh2LinkupCache.Set( SkelMeshName, NewLinkupIndex );
		
		// Fill it up
		FAnimSetMeshLinkup* NewLinkup = &LinkupCache(NewLinkupIndex);
		NewLinkup->BuildLinkup(InSkelMesh, this);

		return NewLinkupIndex;
	}

	return (*IndexPtr);
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT UAnimSet::GetResourceSize()
{
	if (GExclusiveResourceSizeMode)
	{
		// This object only references others, it doesn't have any real resource bytes
		return 0;
	}
	else
	{
		FArchiveCountMem CountBytesSize( this );
		INT ResourceSize = CountBytesSize.GetNum();
		for( INT i=0; i<Sequences.Num(); i++ )
		{
			UAnimSequence* AnimSeq = Sequences(i);
			if( AnimSeq )
			{
				ResourceSize += AnimSeq->GetResourceSize();
			}			
		}
		return ResourceSize;
	}
}

/**
 * Clears all sequences and resets the TrackBoneNames table.
 */
void UAnimSet::ResetAnimSet()
{
	// Make sure we handle AnimSequence references properly before emptying the array.
	for(INT i=0; i<Sequences.Num(); i++)
	{	
		UAnimSequence* AnimSeq = Sequences(i);
		if( AnimSeq )
		{
			AnimSeq->RecycleAnimSequence();
		}
	}
	Sequences.Empty();
	SequenceCache.Empty();
	TrackBoneNames.Empty();
	LinkupCache.Empty();
	SkelMesh2LinkupCache.Empty();

	// We need to re-init any skeleltal mesh components now, because they might still have references to linkups in this set.
	for(TObjectIterator<USkeletalMeshComponent> It;It;++It)
	{
		USkeletalMeshComponent* SkelComp = *It;
		if(!SkelComp->IsPendingKill() && !SkelComp->IsTemplate())
		{
			SkelComp->InitAnimTree();
		}
	}
}

/** 
 * Properly remove an AnimSequence from an AnimSet, and updates references it might have.
 * @return TRUE if AnimSequence was properly removed, FALSE if it wasn't found.
 */
UBOOL UAnimSet::RemoveAnimSequenceFromAnimSet(UAnimSequence* AnimSeq)
{
	INT SequenceIndex = Sequences.FindItemIndex(AnimSeq);
	if( SequenceIndex != INDEX_NONE )
	{
		// Handle reference clean up properly
		AnimSeq->RecycleAnimSequence();
		SequenceCache.Remove(AnimSeq->SequenceName);
		// Remove from array
		Sequences.Remove(SequenceIndex, 1);
		if( GIsEditor )
		{
			MarkPackageDirty();
		}
		return TRUE;
	}

	return FALSE;
}


/** Util that find all AnimSets and flushes their LinkupCache, then calls InitAnimTree on all SkeletalMeshComponents. */
void UAnimSet::ClearAllAnimSetLinkupCaches()
{
	DOUBLE Start = appSeconds();

	TArray<UAnimSet*> AnimSets;
	TArray<USkeletalMeshComponent*> SkelComps;
	// Find all AnimSets and SkeletalMeshComponents (just do one iterator)
	for(TObjectIterator<UObject> It;It;++It)
	{
		UAnimSet* AnimSet = Cast<UAnimSet>(*It);
		if(AnimSet && !AnimSet->IsPendingKill() && !AnimSet->IsTemplate())
		{
			AnimSets.AddItem(AnimSet);
		}

		USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(*It);
		if(SkelComp && !SkelComp->IsPendingKill() && !SkelComp->IsTemplate())
		{
			SkelComps.AddItem(SkelComp);
		}
	}

	// For all AnimSets, empty their linkup cache
	for(INT i=0; i<AnimSets.Num(); i++)
	{
		AnimSets(i)->LinkupCache.Empty();
		AnimSets(i)->SkelMesh2LinkupCache.Empty();
	}

	// For all SkeletalMeshComponents, force anims to be re-bound
	for(INT i=0; i<SkelComps.Num(); i++)
	{
		SkelComps(i)->UpdateAnimations();
	}

	debugf(NAME_DevAnim, TEXT("ClearAllAnimSetLinkupCaches - Took %3.2fms"), (appSeconds() - Start)*1000.f);
}

/*
 * Animation Tracking System - either unused or used and if used, how much used...
 * Per animation and per animset
 */

// Animation information

typedef struct _FAnimationInfo
{
	FString AnimName;
	FString Tag; // temporary tag before we have animsequence tag
	INT		ResourceSize;
	FLOAT	UseScore; 
	UBOOL	Used; // UseScore == 0 does not mean not used - 

	_FAnimationInfo(const FString& InAnimName, INT InResourceSize)
		:	AnimName(InAnimName),
			ResourceSize(InResourceSize), 
			UseScore(0.f), 
			Used(FALSE){}

	void AddScore(FLOAT InScore)
	{
		UseScore += InScore;
	}

	// if Used == FALSE, then set TRUE
	void SetUsed()
	{
		if (Used == FALSE)
		{
			Used = TRUE;
		}
	}
} FAnimationInfo;

// Per loading trace information : Meaning from loading to unloading
// One animset could have been loaded and unloaded multiple time across levels
typedef struct _FLevelAnimSetUsage
{
	PTRINT	InstanceID;			// address, so that I can update information by
	FString LevelName;			// Level Name that was loaded by
	FLOAT	LoadingTime;		// Loading Time - GWorld TimeSeconds
	FLOAT	UnloadingTime;		// Unloading Time - Gworld Time Seconds
	INT		TotalUnusedResourceSize;
	TArray<const FAnimationInfo*>	UnusedAnimations;	// Unused animation information

	_FLevelAnimSetUsage(const FString& InLevelName, FLOAT InLoadingTime, PTRINT InInstanceID) 
		:	InstanceID(InInstanceID),
		LevelName(InLevelName),
		LoadingTime(InLoadingTime), 
		UnloadingTime(0.f),
		TotalUnusedResourceSize(0)
	{
		UnusedAnimations.Empty();
	}
} FLevelAnimSetUsage;

typedef struct _FAnimSetUsage
{
	FString	AnimSetPath;
	INT		TotalNumOfAnimations;	// Num of total animations in the animset
	INT		TotalNumOfUsedAnimations;
	INT		TotalResourceSize;
	INT		TotalResourceSizeOfUsedAnimations;
	FLOAT	AnimsetUseScore;

	TArray<FLevelAnimSetUsage*>	LevelInformation;
	TArray<FAnimationInfo*>		AnimationList;

	_FAnimSetUsage(const FString & InAnimSetPath, INT InTotalNumAnimations)
		:	AnimSetPath(InAnimSetPath),
			TotalNumOfAnimations(InTotalNumAnimations), 
			TotalNumOfUsedAnimations(0), 
			TotalResourceSize(0),
			TotalResourceSizeOfUsedAnimations(0),
			AnimsetUseScore (0.f){} 
} FAnimSetUsage;

// Global debug variable information
// Should be only used if GShouldTraceAnimationUsage is TRUE
TMap<FString, FAnimationInfo*> GAnimationList; // this is unique animation list
TMap<FString, FAnimSetUsage*> GAnimsetUsageMap;
UBOOL GShouldTraceAnimationUsage = FALSE;
UBOOL GBeingTraceAnimationUsage = TRUE;

// Save last saved folder and delete it once new one is created
// So that if any reason, crashed, it will keep the last saved folder
FString GLastFolderSaved;
FLOAT	GLastOutputTime = 0.f;

/*
* return animation info tag
* This is temporary until we add content tag to animation 
* First they search from sequence name, and if nothing is found, they look for animset
* Most of case, animset includes a lot of key information.
*/
FString GetAnimationTag( UAnimSequence * Sequence )
{
	check (Sequence);

	const TArray<FAnimTag> & AnimTags = Sequence->AnimTags;

	FString AnimName = Sequence->SequenceName.GetNameString();
	FString AnimSetName = Sequence->GetAnimSet()->GetName();

	for ( INT I=0; I<AnimTags.Num(); ++I )
	{
		for ( INT J=0; J<AnimTags(I).Contains.Num(); ++J )
		{
			// first found, then return
			// 0-end is priority
			if (AnimName.InStr(AnimTags(I).Contains(J), FALSE, TRUE) != INDEX_NONE)
			{
				return AnimTags(I).Tag;
			}
			// if they don't find it from animname, try animset name
			else if (AnimSetName.InStr(AnimTags(I).Contains(J), FALSE, TRUE) != INDEX_NONE)
			{
				return AnimTags(I).Tag;
			}
		}
	}

	// default
	return TEXT("NONE");
}

/*
 * Get FAnimationInfo from Sequence
 * If not found, create one.
 */
FAnimationInfo* GetAnimationInfo( UAnimSequence * Sequence )
{
	check (GShouldTraceAnimationUsage);

	if ( Sequence )
	{
		FAnimationInfo ** Found = GAnimationList.Find(Sequence->GetPathName());
		if ( Found == NULL )
		{
			FAnimationInfo * NewInfo = new FAnimationInfo(Sequence->SequenceName.GetNameString(), Sequence->GetResourceSize());
			GAnimationList.Set(Sequence->GetPathName(), NewInfo);
			NewInfo->Tag = GetAnimationTag(Sequence);
			return NewInfo;
		}

		return *Found;
	}

	return NULL;
}

/*
* Get FAnimSetUsage from AnimSet
* If not found, create one.
*/
FAnimSetUsage* GetAnimSetUsage( UAnimSet * AnimSet )
{
	check (GShouldTraceAnimationUsage);

	if ( AnimSet )
	{
		// Get AnimSetUsage that has same path name with this
		FAnimSetUsage ** Found = GAnimsetUsageMap.Find(AnimSet->GetPathName());
		if ( Found == NULL )
		{
			FAnimSetUsage * NewUsage = new FAnimSetUsage(AnimSet->GetPathName(), AnimSet->Sequences.Num());
			// Enter all animsequence for this animset
			for ( INT I=0; I<AnimSet->Sequences.Num(); ++I )
			{
				NewUsage->AnimationList.AddItem(GetAnimationInfo(AnimSet->Sequences(I)));
			}

			// add to map
			GAnimsetUsageMap.Set(AnimSet->GetPathName(), NewUsage);

			return NewUsage;
		}

		return *Found;
	}

	return NULL;
}

/**
* Add animation usage information to TMap
*/
void UAnimSet::TraceAnimationUsage()
{
	check (GShouldTraceAnimationUsage);

	if (GBeingTraceAnimationUsage == FALSE)
	{
		return;
	}

	FAnimSetUsage * Current = GetAnimSetUsage( this );

	check (Current);

	if ( GWorld )
	{
		if ( GWorld->PersistentLevel )
		{
			Current->LevelInformation.AddItem(new FLevelAnimSetUsage(GWorld->PersistentLevel->GetPathName(), GWorld->GetTimeSeconds(), (PTRINT)this));
		}
		else 
		{
			Current->LevelInformation.AddItem(new FLevelAnimSetUsage(TEXT("No Persistent Level"), GWorld->GetTimeSeconds(), (PTRINT)this));
		}
	}
	else
	{
		Current->LevelInformation.AddItem(new FLevelAnimSetUsage(TEXT("No Persistent Level"), 0.0f, (PTRINT)this));
	}
}

/**
* Record Animation Usage
*/
void UAnimSet::RecordAnimationUsage()
{
	check (GShouldTraceAnimationUsage);

	if (GBeingTraceAnimationUsage == FALSE)
	{
		return;
	}

	FAnimSetUsage* Current = GetAnimSetUsage( this );

	// there is chance it might not be there when exiting
	check (Current);

	// find current animset ones
	for( INT I=0; I<Current->LevelInformation.Num(); ++I )
	{
		// find this instance
		if (Current->LevelInformation(I)->InstanceID == (PTRINT)this)
		{
			// get the level data
			FLevelAnimSetUsage* SrcUsage = Current->LevelInformation(I);
			if (SrcUsage)
			{
				SrcUsage->UnloadingTime = (GWorld)?GWorld->GetTimeSeconds():0.0f;
				// clear
				SrcUsage->UnusedAnimations.Empty();
				SrcUsage->TotalUnusedResourceSize = 0;

				for (INT J=0; J<Sequences.Num(); ++J)
				{
					FAnimationInfo * Found = GetAnimationInfo(Sequences(J));

					// add score, and clear sequence score to avoid multiple additions
					Found->AddScore(Sequences(J)->UseScore);
					Sequences(J)->UseScore = 0;

					// if haven't been used, add item
					if (Sequences(J)->bHasBeenUsed == FALSE)
					{
						SrcUsage->UnusedAnimations.AddItem(Found);
						SrcUsage->TotalUnusedResourceSize+=Found->ResourceSize;
					}
					else
					{
						// Mark used
						Found->SetUsed();
					}
				}
			}

			break;
		}
	}
}

void UAnimSet::CleanUpAnimationUsage()
{
	// free all allocated memory
	for (TMap<FString, FAnimationInfo*>::TIterator Iter(GAnimationList); Iter; ++Iter) // this is unique animation list
	{
		delete Iter.Value();
	}

	GAnimationList.Empty();

	for (TMap<FString, FAnimSetUsage*>::TIterator Iter(GAnimsetUsageMap); Iter; ++Iter) 
	{
		FAnimSetUsage * AnimSetUsage = Iter.Value();

		for ( INT I=0; I<AnimSetUsage->LevelInformation.Num(); ++I )
		{
			delete AnimSetUsage->LevelInformation(I);
		}

		delete AnimSetUsage;
	}

	GAnimsetUsageMap.Empty();

	// We'd like to avoid more allocation after animation usage is cleared, (Right now in PreExit)
	// Right now this is only per game session. If you'd like to make it
	// multiple session per game, then make sure you turn this flag on when start capturing again
	GBeingTraceAnimationUsage = FALSE;
}

/** 
 *  Output if time is up
 */

void UAnimSet::TickAnimationUsage()
{
	if ( GWorld )
	{
		// if more than 10 mins, record...
		if (GWorld->GetTimeSeconds() - GLastOutputTime > 600)
		{
			OutputAnimationUsage();
			GLastOutputTime = GWorld->GetTimeSeconds();
		}
	}

}
/**
* Output Animation Usage
*/ 
void UAnimSet::OutputAnimationUsage()
{
	// if the animset hasn't been closed yet, go over and close it up.
	if ( GShouldTraceAnimationUsage && GAnimsetUsageMap.Num()>0 )
	{
		// go through recording since I'm about to output
		for(TObjectIterator<UAnimSet> It;It;++It)
		{
			UAnimSet * Current = *It;
			Current->RecordAnimationUsage();
		}

		const FString CurrentTime = appSystemTimeString();

		/// create base folder
		GFileManager->MakeDirectory( *FString(appGameLogDir() + TEXT("AnimationUsage")) );

		// create current folder
		const FString CSVDirectory = appGameLogDir() + TEXT("AnimationUsage") + PATH_SEPARATOR + FString::Printf( TEXT("%s-%d-%s"), GGameName, GetChangeListNumberForPerfTesting(), *CurrentTime ) + PATH_SEPARATOR;
		GFileManager->MakeDirectory( *CSVDirectory );

		// fill up empty data first before output
		for ( TMap<FString, FAnimSetUsage*>::TConstIterator Iter(GAnimsetUsageMap); Iter; ++Iter )
		{
			FAnimSetUsage * AnimSet = Iter.Value();
			AnimSet->AnimsetUseScore = 0.f;
			AnimSet->TotalNumOfUsedAnimations = 0;
			AnimSet->TotalResourceSizeOfUsedAnimations = 0;
			AnimSet->TotalResourceSize = 0;

			for (INT I=0; I<AnimSet->AnimationList.Num(); ++I)
			{
				// get total use score to get idea 
				AnimSet->AnimsetUseScore += AnimSet->AnimationList(I)->UseScore;
				AnimSet->TotalResourceSize += AnimSet->AnimationList(I)->ResourceSize;

				if (AnimSet->AnimationList(I)->Used)
				{
					AnimSet->TotalNumOfUsedAnimations++;
					AnimSet->TotalResourceSizeOfUsedAnimations += AnimSet->AnimationList(I)->ResourceSize;
				}
			}
		}

		// go through two phase
		// first phase, just output unused animations
		for ( TMap<FString, FAnimSetUsage*>::TConstIterator Iter(GAnimsetUsageMap); Iter; ++Iter )
		{
			FAnimSetUsage * AnimSet = Iter.Value();
			FArchive*	CSVFile	= NULL;
			const FString CSVFilename	= FString::Printf(TEXT("%s%s%s.csv"), *CSVDirectory, PATH_SEPARATOR, *AnimSet->AnimSetPath);

			// find current animset ones
			// write the header first
			// go through all level info and write it up first
			for( INT I=0; I<AnimSet->LevelInformation.Num(); ++I )
			{
				FLevelAnimSetUsage* Usage = AnimSet->LevelInformation(I);

				// No need to serialize if no unused animations, so defer the serialization until it's found
				if (Usage->UnusedAnimations.Num() > 0 )			
				{
					FString Row;

					// if that hasn't been created, create one for this animset
					if ( CSVFile==NULL )
					{
						CSVFile	= GFileManager->CreateFileWriter( *CSVFilename );

						Row = TEXT("AnimSet,LevelName,LoadingTime,UnloadingTime,Total Number of Animation,Unused Percentage, Unused Resource Percentage,Unused Animations,Unused Resource Size") LINE_TERMINATOR;
						CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
					}

					Row = FString::Printf(TEXT("%s,%s,%0.2f,%0.2f,%d,%d,%d,%d,%d%s"), *AnimSet->AnimSetPath,*Usage->LevelName,Usage->LoadingTime,Usage->UnloadingTime,
						AnimSet->TotalNumOfAnimations, (INT)(100*(Usage->UnusedAnimations.Num())/AnimSet->TotalNumOfAnimations), (INT)(100*(Usage->TotalUnusedResourceSize)/AnimSet->TotalResourceSize),
						Usage->UnusedAnimations.Num(), Usage->TotalUnusedResourceSize, LINE_TERMINATOR);

					CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );

					for (INT J=0; J<Usage->UnusedAnimations.Num(); ++J)
					{
						Row = FString::Printf(TEXT(",,,,,,,%s,%d%s"), *Usage->UnusedAnimations(J)->AnimName, Usage->UnusedAnimations(J)->ResourceSize, LINE_TERMINATOR);
						CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
					}
				}
			}

			// if file has been created for this animset, close it now. 
			if ( CSVFile )
			{
				CSVFile->Close();
				delete CSVFile;
				CSVFile	= NULL;
			}
		}

		// once that's done, create summary in the second phase
		// Set AnimSetUsage UseScore
		FString CSVFilename	= FString::Printf(TEXT("%s%sAnimSetUsageSummary.csv"), *CSVDirectory, PATH_SEPARATOR);
		FArchive*	CSVFile = GFileManager->CreateFileWriter( *CSVFilename );
		FString Row = TEXT("AnimSet,Number of Loadings,Total Number of Animation,Total Usage Score,Used Percentage,Unused Resource Percentage,Unused Resource Size,Total Resource Size") LINE_TERMINATOR;

		CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );

		for ( TMap<FString, FAnimSetUsage*>::TConstIterator Iter(GAnimsetUsageMap); Iter; ++Iter )
		{
			FAnimSetUsage * AnimSet = Iter.Value();

			Row = FString::Printf(TEXT("%s,%d,%d,%0.2f,%d,%d,%d,%d%s"), *AnimSet->AnimSetPath,  AnimSet->LevelInformation.Num(),  
				AnimSet->TotalNumOfAnimations, AnimSet->AnimsetUseScore, (AnimSet->TotalNumOfUsedAnimations>0)?(INT)(100*AnimSet->TotalNumOfUsedAnimations/AnimSet->TotalNumOfAnimations):0, 
				(AnimSet->TotalResourceSizeOfUsedAnimations>0)?(INT)((100*(AnimSet->TotalResourceSize-AnimSet->TotalResourceSizeOfUsedAnimations))/AnimSet->TotalResourceSize):0,
				(AnimSet->TotalResourceSize-AnimSet->TotalResourceSizeOfUsedAnimations), AnimSet->TotalResourceSize, LINE_TERMINATOR);

			CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
		}

		CSVFile->Close();
		delete CSVFile;

		CSVFilename	= FString::Printf(TEXT("%s%sAnimSequenceUsageSummary.csv"), *CSVDirectory, PATH_SEPARATOR);
		CSVFile = GFileManager->CreateFileWriter( *CSVFilename );
		Row = TEXT("AnimSet,Animation,Tag,Score,Resource Size,Not Used") LINE_TERMINATOR;

		CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );

		for ( TMap<FString, FAnimSetUsage*>::TConstIterator Iter(GAnimsetUsageMap); Iter; ++Iter )
		{
			FAnimSetUsage * AnimSet = Iter.Value();

			for (INT I=0; I<AnimSet->AnimationList.Num(); ++I)
			{
				Row = FString::Printf(TEXT("%s,%s,%s,%0.2f,%d,%d%s"), *AnimSet->AnimSetPath,  *AnimSet->AnimationList(I)->AnimName,  *AnimSet->AnimationList(I)->Tag, 
					AnimSet->AnimationList(I)->UseScore, AnimSet->AnimationList(I)->ResourceSize, (AnimSet->AnimationList(I)->Used)?0:1, LINE_TERMINATOR);

				CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
			}
		}

		CSVFile->Close();
		delete CSVFile;

		// Now delete previous saved folder and update LastSavedFolder
		// Before moving forward, delete previous folder
		if ( GLastFolderSaved!=TEXT(""))
		{
			GFileManager->DeleteDirectory(*GLastFolderSaved, TRUE, TRUE);
		}

		GLastFolderSaved = CSVDirectory;
	}
}
/*-----------------------------------------------------------------------------
	AnimNotify subclasses
-----------------------------------------------------------------------------*/

//
// UAnimNotify_Sound
//
void UAnimNotify_Sound::Notify( UAnimNodeSequence* NodeSeq )
{
	// BM: not ported yet - BM2 posts EventName through the owner's AkComponent, gated on CharacterFilter
}
IMPLEMENT_CLASS(UAnimNotify_Sound);

IMPLEMENT_CLASS(UAnimNotify_PawnMaterialParam);

//
// UAnimNotify_Script
//

struct FAnimNotifierHandler_Parms
{
	FLOAT	CurrentTime;
	FLOAT	TotalDuration;
	FAnimNotifierHandler_Parms(EEventParm)
	{
	}
};

void FindAndCallFunctionOnActor(AActor *Owner, FName NotifyName, FLOAT CurrentTime=0.f, FLOAT TotalDuration=0.f)
{
	if( Owner && NotifyName != NAME_None )
	{
		if( !GWorld->HasBegunPlay() )
		{
			//shhhhh... debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Script %s"), *NotifyName.ToString() );
		}
		else
		{
			//warnf( TEXT("UAnimNotify_Script: %s %s %s"), *NodeSeq->SkelComponent->GetDetailedInfo(), *NotifyName.ToString(), *Owner->GetName() );
			UFunction* Function = Owner->FindFunction( NotifyName );
			if( Function )
			{
				if( Function->FunctionFlags & FUNC_Delegate )
				{
					UDelegateProperty* DelegateProperty = FindField<UDelegateProperty>( Owner->GetClass(), *FString::Printf(TEXT("__%s__Delegate"),*NotifyName.ToString()) );
					FScriptDelegate* ScriptDelegate = (FScriptDelegate*)((BYTE*)Owner + DelegateProperty->Offset);
                    Owner->ProcessDelegate( NotifyName, ScriptDelegate, NULL );
				}
				else 
				{
					// if parameter is none, add event
					if ( Function->NumParms == 0 )
					{
						Owner->ProcessEvent( Function, NULL );								
					}
					// if parameter exists and are floats, send parameter
					else if ( Function->NumParms == 2 &&  // num parameter is 1
						Cast<UFloatProperty>(Function->PropertyLink) != NULL &&  // make sure that's animnotifi)
						Cast<UFloatProperty>(Function->PropertyLink->PropertyLinkNext) != NULL)
					{
						FAnimNotifierHandler_Parms Parms(EC_EventParm);
						Parms.CurrentTime = CurrentTime;
						Parms.TotalDuration = TotalDuration;
						Owner->ProcessEvent( Function, &Parms );								
					}
					else
					{
						// Actor has event, but with different parameters. Print warning
						debugf(NAME_Warning, TEXT("Actor %s has a anim notifier named %s, but the parameter number does not match or not of the correct type (should have one or zero and if one, it should be AnimNotify_Script)"), *Owner->GetName(), *NotifyName.ToString());
					}
				}
			}
			else
			{
				debugf(NAME_Warning,TEXT("Failed to find notify %s on %s"),*NotifyName.ToString(),*Owner->GetName());
			}
		}
	}
}

void UAnimNotify_Script::Notify( UAnimNodeSequence* NodeSeq )
{
	FindAndCallFunctionOnActor(NodeSeq->SkelComponent->GetOwner(),NotifyName);
	//debugf(TEXT("%.4f %s begin"),GWorld->GetTimeSeconds(),*NotifyName.ToString());
}

void UAnimNotify_Script::NotifyTick( UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime, FLOAT AnimTimeStep, FLOAT InTotalDuration )
{
	FindAndCallFunctionOnActor(NodeSeq->SkelComponent->GetOwner(),NotifyTickName, AnimCurrentTime, InTotalDuration);
	//debugf(TEXT("%.4f %s %.3f"),GWorld->GetTimeSeconds(),*NotifyTickName.ToString(),AnimTimeStep);
}

void UAnimNotify_Script::NotifyEnd( UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime )
{
	FindAndCallFunctionOnActor(NodeSeq->SkelComponent->GetOwner(),NotifyEndName);
	//debugf(TEXT("%.4f %s end"),GWorld->GetTimeSeconds(),*NotifyEndName.ToString());
}
IMPLEMENT_CLASS(UAnimNotify_Script);

//
// UAnimNotify_Scripted
//
void UAnimNotify_Scripted::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	if( Owner )
	{
		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Scripted %s"), *GetName() );
		}
		else
		{
			eventNotify( Owner, NodeSeq );
		}
	}
}

void UAnimNotify_Scripted::NotifyEnd( class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime )
{
	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	if( Owner )
	{
		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Scripted %s"), *GetName() );
		}
		else
		{
			eventNotifyEnd( Owner, NodeSeq );
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_Scripted);

//
// UAnimNotify_Kismet
//
void UAnimNotify_Kismet::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	if( Owner && NotifyName != NAME_None )
	{
		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Kismet %s"), *NotifyName.ToString() );
		}
		else
		{
			USeqEvent_AnimNotify* Evt;
			for( INT EventIdx = 0; EventIdx < Owner->GeneratedEvents.Num(); ++EventIdx )
			{
				Evt = Cast<USeqEvent_AnimNotify>(Owner->GeneratedEvents(EventIdx));
				if( Evt && NotifyName == Evt->NotifyName )
				{
					Evt->CheckActivate(Owner, Owner, FALSE);
				}
			}
		}
	}

}
IMPLEMENT_CLASS(UAnimNotify_Kismet);

//
// UAnimNotify_Footstep
//
void UAnimNotify_Footstep::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = (NodeSeq && NodeSeq->SkelComponent) ? NodeSeq->SkelComponent->GetOwner() : NULL;

	if( !Owner )
	{
		// This should not be the case in the game, so generate a warning.
		if( GWorld->HasBegunPlay() )
		{
			debugf(TEXT("FOOTSTEP no owner"));
		}
	}
	else
	{
		//debugf(TEXT("FOOTSTEP for %s"),*Owner->GetName());

		// Act on footstep...  FootDown signifies which paw hits earth 0=left, 1=right, 2=left-hindleg etc.
		if( Owner->GetAPawn() )
		{
			Owner->GetAPawn()->eventPlayFootStepSound(FootDown);
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_Footstep);


//
// AnimNotify_CameraEffect
//
void UAnimNotify_CameraEffect::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	if( Owner )
	{
		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_CameraEffect %s"), *GetName() );
		}
		else
		{
			if( ( Owner->GetAPawn() != NULL )
				&& ( Cast<APlayerController>(Owner->GetAPawn()->Controller) != NULL )
				)
			{
				Cast<APlayerController>(Owner->GetAPawn()->Controller)->eventClientSpawnCameraLensEffect( CameraLensEffect );
			}
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_CameraEffect);


//
// AnimNotify_PlayParticleEffect
//
void UAnimNotify_PlayParticleEffect::Notify( UAnimNodeSequence* NodeSeq )
{
	// Don't bother trying if the template is null...
	if( PSTemplate == NULL || DEDICATED_SERVER )
	{
#if DEDICATED_SERVER
		debugf(TEXT("AnimNotify::PlayParticleEffect Anim:%s Template: %s"), *NodeSeq->AnimSeqName.ToString(), PSTemplate ? *PSTemplate->GetPathName() : TEXT("None") );
#endif
		return;
	}

	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	
	// Skip if Owner is hidden.
	if( bSkipIfOwnerIsHidden 
		&& ((Owner && Owner->bHidden) || NodeSeq->SkelComponent->HiddenGame) )
	{
		return;
	}

	UBOOL bPlayedEffect = FALSE;
	// try owner first
	if( Owner )
	{
		if( Owner->bHidden )
		{
			bSkipIfOwnerIsHidden = TRUE;
		}

		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_PlayParticleEffect %s"), *GetName() );
		}
		else
		{
			bPlayedEffect = Owner->eventPlayParticleEffect( this );
		}
	}
	
	// If we are showing in the editor, or play has begun and there is no owner
	if (bPlayedEffect == FALSE)
	{
		// Play it on the skeletal mesh
		NodeSeq->SkelComponent->eventPlayParticleEffect( this );
	}
}
IMPLEMENT_CLASS(UAnimNotify_PlayParticleEffect);

//
// AnimNotify_SetMaxDistanceScale
//
void UAnimNotify_ClothingMaxDistanceScale::Notify( UAnimNodeSequence* NodeSeq )
{
	Super::Notify(NodeSeq);
	FLOAT PlayRate = NodeSeq->GetGlobalPlayRate();
	FLOAT ScaledDuration = (PlayRate > 0) ? (1.0f / PlayRate) * Duration : 0.0f;
	NodeSeq->SkelComponent->SetApexClothingMaxDistanceScale(StartScale, EndScale, static_cast<EMaxDistanceScaleMode>(ScaleMode), ScaledDuration);
}

void UAnimNotify_ClothingMaxDistanceScale::NotifyEnd( class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime )
{
	Super::NotifyEnd(NodeSeq, AnimCurrentTime);
	NodeSeq->SkelComponent->SetApexClothingMaxDistanceScale(EndScale, EndScale, static_cast<EMaxDistanceScaleMode>(ScaleMode), 0.0f);
}

IMPLEMENT_CLASS(UAnimNotify_ClothingMaxDistanceScale);

//
// AnimNotify_Rumble
//
void UAnimNotify_Rumble::Notify( UAnimNodeSequence* NodeSeq )
{
	AActor* Owner = NodeSeq->SkelComponent->GetOwner();
	if( Owner )
	{
		if( !GWorld->HasBegunPlay() )
		{
			debugf( NAME_Log, TEXT("Editor: skipping AnimNotify_Rumble %s"), *GetName() );
		}
		else
		{
			if( bCheckForBasedPlayer || EffectRadius > 0.0 )
			{
				for( INT PlayerIndex = 0; PlayerIndex < GEngine->GamePlayers.Num(); PlayerIndex++ )
				{
					if( GEngine->GamePlayers(PlayerIndex) )
					{
						if( !GEngine->GamePlayers(PlayerIndex)->Actor || !GEngine->GamePlayers(PlayerIndex)->Actor->Pawn )
							continue;
						else
						{
							if( bCheckForBasedPlayer )
							{
								if( GEngine->GamePlayers(PlayerIndex)->Actor->Pawn->IsBasedOn(Owner) )
								{
									Owner = GEngine->GamePlayers(PlayerIndex)->Actor->Pawn;
									break;
								}
							}
							else
							{
								FLOAT fdistSq = (Owner->Location - GEngine->GamePlayers(PlayerIndex)->Actor->Pawn->Location).SizeSquared();
								if(fdistSq <= EffectRadius * EffectRadius)
								{
									//found the player and he is based on us
									Owner = GEngine->GamePlayers(PlayerIndex)->Actor->Pawn;
									break;
								}
							}
						}
					}
				}
			}

			if( ( Owner->GetAPawn() != NULL )
				&& ( Cast<APlayerController>(Owner->GetAPawn()->Controller) != NULL )
				)
			{
				Cast<APlayerController>(Owner->GetAPawn()->Controller)->eventPlayRumble( this );
			}
		}
	}
}
IMPLEMENT_CLASS(UAnimNotify_Rumble);
IMPLEMENT_CLASS(UWaveFormBase);

void UAnimNotify_ViewShake::PostLoad()
{
	// upgrade old format data to the new format
	if ( (!RotAmplitude.IsZero() || !LocAmplitude.IsZero() || FOVAmplitude != 0.f) && (ShakeParams == NULL) )
	{
		// need to upgrade
		ShakeParams = Cast<UCameraShake>(StaticConstructObject(UCameraShake::StaticClass(), this));
		if (ShakeParams)
		{
			// copy data to new format
			ShakeParams->OscillationDuration = Duration;

			ShakeParams->RotOscillation.Pitch.Amplitude = RotAmplitude.X;
			ShakeParams->RotOscillation.Pitch.Frequency = RotFrequency.X;
			ShakeParams->RotOscillation.Yaw.Amplitude = RotAmplitude.Y;
			ShakeParams->RotOscillation.Yaw.Frequency = RotFrequency.Y;
			ShakeParams->RotOscillation.Roll.Amplitude = RotAmplitude.Z;
			ShakeParams->RotOscillation.Roll.Frequency = RotFrequency.Z;

			ShakeParams->LocOscillation.X.Amplitude = LocAmplitude.X;
			ShakeParams->LocOscillation.X.Frequency = LocFrequency.X;
			ShakeParams->LocOscillation.Y.Amplitude = LocAmplitude.Y;
			ShakeParams->LocOscillation.Y.Frequency = LocFrequency.Y;
			ShakeParams->LocOscillation.Z.Amplitude = LocAmplitude.Z;
			ShakeParams->LocOscillation.Z.Frequency = LocFrequency.Z;

			ShakeParams->FOVOscillation.Amplitude = FOVAmplitude;
			ShakeParams->FOVOscillation.Frequency = FOVFrequency;
		}

		// zero out old data so we don't upgrade again
		RotAmplitude = FVector::ZeroVector;
		RotFrequency = FVector::ZeroVector;
		LocAmplitude = FVector::ZeroVector;
		LocFrequency = FVector::ZeroVector;
		FOVAmplitude = 0.f;
		FOVFrequency = 0.f;

		MarkPackageDirty(TRUE);
	}

	Super::PostLoad();
}
IMPLEMENT_CLASS(UAnimNotify_ViewShake);

INT UAnimNotify_Trails::GetNumSteps(INT InLastTrailIndex) const
{
	if ((CurrentTime < 0.0f) || (InLastTrailIndex == -1))
	{
		return 0;
	}

	FLOAT CurrEndTime = CurrentTime + TimeStep;
	INT NumSteps = 0;
	if ((InLastTrailIndex + 1) < TrailSampledData.Num())
	{
		const FTrailSample& CheckStartAnimSamplePoint = TrailSampledData(InLastTrailIndex);
		FLOAT CurrSampledTime = LastStartTime + CheckStartAnimSamplePoint.RelativeTime;
		while (CurrEndTime >= CurrSampledTime)
		{
			if ((InLastTrailIndex + NumSteps + 1) < TrailSampledData.Num())
			{
				NumSteps++;
				const FTrailSample& CheckAnimSamplePoint = TrailSampledData(InLastTrailIndex + NumSteps);
				CurrSampledTime = LastStartTime + CheckAnimSamplePoint.RelativeTime;
			}
			else
			{
				// We have hit the end of available sample data... but not worthy of warning.
				break;
			}
		}
	}

	return NumSteps;
}

// UObject interface.
void UAnimNotify_Trails::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UBOOL bResampleAnimation = FALSE;
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetName() == TEXT("EndTime"))
		{
			bResampleAnimation = TRUE;
		}
		else if (PropertyThatChanged->GetName() == TEXT("SamplesPerSecond"))
		{
			// By default, make 200 FPS the max sample rate.
			// Allow the INI setting to override this.
			FLOAT MaxSampleRate = 200.0f;
			GConfig->GetFloat(TEXT("AnimNotify"), TEXT("Trail_MaxSampleRate"), MaxSampleRate, GEngineIni);
			SamplesPerSecond = Clamp<FLOAT>(SamplesPerSecond, 0.01f, MaxSampleRate);
			bResampleAnimation = TRUE;
		}
		else if (PropertyThatChanged->GetName() == TEXT("FirstEdgeSocketName"))
		{
			bResampleAnimation = TRUE;
		}
		else if (PropertyThatChanged->GetName() == TEXT("SecondEdgeSocketName"))
		{
			bResampleAnimation = TRUE;
		}
		else if (PropertyThatChanged->GetName() == TEXT("ControlPointSocketName"))
		{
			bResampleAnimation = TRUE;
		}
	}

	if (bResampleAnimation == TRUE)
	{
		//@todo. How to get the anim sequence now????
	}
}

void UAnimNotify_Trails::PostLoad()
{
	Super::PostLoad();
	if (GetLinkerVersion() < VER_ANIMNOTIFY_TRAIL_SAMPLEFRAMERATE)
	{
		SamplesPerSecond = 1.0f / SampleTimeStep;
	}

	if ((GetLinkerVersion() < VER_ANIMNOTIFY_TRAILS_REMOVED_VELOCITY) && (IsTemplate() == FALSE))
	{
		// Copy the results from the old to the new
		TrailSampledData.Empty(TrailSampleData.Num());
		TrailSampledData.AddZeroed(TrailSampleData.Num());
		for (INT CopyIdx = 0; CopyIdx < TrailSampleData.Num(); CopyIdx++)
		{
			FTrailSamplePoint& SrcSample = TrailSampleData(CopyIdx);
			FTrailSample& DestSample = TrailSampledData(CopyIdx);

			DestSample.RelativeTime = SrcSample.RelativeTime;
			DestSample.FirstEdgeSample = SrcSample.FirstEdgeSample.Position;
			DestSample.SecondEdgeSample = SrcSample.SecondEdgeSample.Position;
			DestSample.ControlPointSample = SrcSample.ControlPointSample.Position;
		}
		TrailSampleData.Empty();
	}
}

AActor* UAnimNotify_Trails::GetNotifyActor(class UAnimNodeSequence* NodeSeq)
{
	check(NodeSeq);
	return (NodeSeq->SkelComponent ? NodeSeq->SkelComponent->GetOwner() : NULL);
}

// AnimNotify interface.
void UAnimNotify_Trails::Notify(class UAnimNodeSequence* NodeSeq)
{
	Super::Notify(NodeSeq);

	AnimNodeSeq = NodeSeq;
	CurrentTime = LastStartTime;
	TimeStep = 0.0f;

	HandleNotify(NodeSeq, TrailNotifyType_Start);
}

void UAnimNotify_Trails::NotifyTick(class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime, FLOAT AnimTimeStep, FLOAT InTotalDuration)
{
	Super::NotifyTick(NodeSeq, AnimCurrentTime, AnimTimeStep, InTotalDuration);

	AnimNodeSeq = NodeSeq;
	CurrentTime = AnimCurrentTime;
	TimeStep = AnimTimeStep;

	HandleNotify(NodeSeq, TrailNotifyType_Tick);
}

void UAnimNotify_Trails::NotifyEnd(class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime)
{
	Super::NotifyEnd(NodeSeq, AnimCurrentTime);

	AnimNodeSeq = NodeSeq;
	TimeStep = CurrentTime - EndTime;
	CurrentTime = AnimCurrentTime;

	HandleNotify(NodeSeq, TrailNotifyType_End);
}

/**
 *	Handle the various notifies. This should only be called internally!
 *
 *	@param	InNodeSeq		The anim node sequence triggering the notify
 *	@param	InNotifyType	The type of notify that is being handled
 */
void UAnimNotify_Trails::HandleNotify(class UAnimNodeSequence* InNodeSeq, ETrailNotifyType InNotifyType)
{
	check((InNotifyType == TrailNotifyType_Start) || (InNotifyType == TrailNotifyType_Tick) || (InNotifyType == TrailNotifyType_End));

	AActor* Owner = GetNotifyActor(InNodeSeq);
	if (Owner)
	{
		if (GWorld->HasBegunPlay())
		{
			switch (InNotifyType)
			{
			case TrailNotifyType_Start:
				Owner->eventTrailsNotify(this);
				break;
			case TrailNotifyType_Tick:
				Owner->eventTrailsNotifyTick(this);
				break;
			case TrailNotifyType_End:
				Owner->eventTrailsNotifyEnd(this);
				break;
			}
		}
	}

	const UBOOL bShowInEditor = GIsEditor && bPreview && !GIsGame /*&& bAttach && ((SocketName != NAME_None) || (BoneName != NAME_None))*/;
	if (GWorld->HasBegunPlay() || bShowInEditor)
	{
		if (PSTemplate != NULL)
		{
			// Skip if Owner is hidden.
			if (!(bSkipIfOwnerIsHidden && ((Owner && Owner->bHidden) || InNodeSeq->SkelComponent->HiddenGame)))
			{
				FMatrix SavedLocalToWorld(FMatrix::Identity);
				if (bShowInEditor == TRUE)
				{
					SavedLocalToWorld = InNodeSeq->SkelComponent->LocalToWorld;
					FTranslationMatrix TransMat(FVector(0.f));
					InNodeSeq->SkelComponent->LocalToWorld *= TransMat;
				}

				UParticleSystemComponent* PSysComp = GetPSysComponent(InNodeSeq);
				if ((PSysComp == NULL) && (InNotifyType == TrailNotifyType_Start))
				{
					PSysComp = ConstructObject<UParticleSystemComponent>(UParticleSystemComponent::StaticClass(), InNodeSeq->SkelComponent);
					InNodeSeq->SkelComponent->AttachComponentToSocket(PSysComp, ControlPointSocketName);
					PSysComp->SetTemplate(PSTemplate);
					PSysComp->SetTickGroup(TG_PostUpdateWork);
				}

				if (PSysComp)
				{
					switch (InNotifyType)
					{
					case TrailNotifyType_Start:
						PSysComp->ActivateSystem(TRUE);
						PSysComp->TrailsNotify(this);
						break;
					case TrailNotifyType_Tick:
						PSysComp->TrailsNotifyTick(this);
						break;
					case TrailNotifyType_End:
						PSysComp->TrailsNotifyEnd(this);
						break;
					}
				}

				if (bShowInEditor == TRUE)
				{
					InNodeSeq->SkelComponent->LocalToWorld = SavedLocalToWorld;
				}
			}
		}
	}

	// Clear the reference to the anim node seq
	AnimNodeSeq = NULL;
}

/** 
 *	Find the ParticleSystemComponent used by this anim notify.
 *
 *	@param	NodeSeq						The AnimNodeSequence this notify is associated with.
 *
 *	@return	UParticleSystemComponent	The particle system component
 */
UParticleSystemComponent* UAnimNotify_Trails::GetPSysComponent(class UAnimNodeSequence* NodeSeq)
{
	if (NodeSeq && NodeSeq->SkelComponent)
	{
		for (INT AttachmentIdx = 0; AttachmentIdx < NodeSeq->SkelComponent->Attachments.Num(); AttachmentIdx++)
		{
			UParticleSystemComponent* PSysComp = Cast<UParticleSystemComponent>(NodeSeq->SkelComponent->Attachments(AttachmentIdx).Component);
			if (PSysComp)
			{
				if (PSysComp->Template == PSTemplate)
				{
					return PSysComp;
				}
			}
		}
	}

	return NULL;
}

/** Verify the notify is setup correctly for sampling animation data. Editor-only. */
UBOOL UAnimNotify_Trails::IsSetupValid(class UAnimNodeSequence* NodeSeq)
{
	UBOOL bBadSampleState = FALSE;

	FString ErrorMessage;

	// Check for valid mesh and sockets...
	if (NodeSeq != NULL)
	{
		if ((NodeSeq->SkelComponent == NULL) || (NodeSeq->SkelComponent->SkeletalMesh == NULL))
		{
			bBadSampleState = TRUE;
			ErrorMessage = LocalizeUnrealEd("InvalidSkeletalMesh");
		}
		else
		{
			USkeletalMesh* SkelMesh = NodeSeq->SkelComponent->SkeletalMesh;
			UAnimSequence* AnimSeq = NodeSeq->AnimSeq;
			if (AnimSeq == NULL)
			{
				bBadSampleState = TRUE;
				ErrorMessage = LocalizeUnrealEd("InvalidAnimSequence");
			}
			else
			{
				// Check for sockets...
				if ((FirstEdgeSocketName == NAME_None) ||
					(SecondEdgeSocketName == NAME_None) ||
					(ControlPointSocketName == NAME_None))
				{
					bBadSampleState = TRUE;
					ErrorMessage = LocalizeUnrealEd("AnimNotify_Trails_MissingSocketNames");
				}
				else
				{
					if (SkelMesh->FindSocket(FirstEdgeSocketName) == NULL)
					{
						bBadSampleState = TRUE;
						ErrorMessage = FString::Printf(LocalizeSecure(LocalizeUnrealEd("AnimNotify_Trails_MissingSocketOnSkelMesh"), *(FirstEdgeSocketName.ToString())));
					}
					else if (SkelMesh->FindSocket(SecondEdgeSocketName) == NULL)
					{
						bBadSampleState = TRUE;
						ErrorMessage = FString::Printf(LocalizeSecure(LocalizeUnrealEd("AnimNotify_Trails_MissingSocketOnSkelMesh"), *(SecondEdgeSocketName.ToString())));
					}
					else if (SkelMesh->FindSocket(ControlPointSocketName) == NULL)
					{
						bBadSampleState = TRUE;
						ErrorMessage = FString::Printf(LocalizeSecure(LocalizeUnrealEd("AnimNotify_Trails_MissingSocketOnSkelMesh"), *(ControlPointSocketName.ToString())));
					}
				}
			}
		}
	}

	if (bBadSampleState == TRUE)
	{
		FString DisplayError = LocalizeUnrealEd("AnimNotify_Trails_SkippingError");
		DisplayError += TEXT("\n");
		DisplayError += ErrorMessage;
		appMsgf(AMT_OK, *DisplayError);
	}

	return !bBadSampleState;
}

IMPLEMENT_CLASS(UAnimNotify_Trails);

////////////

void UAnimNotify_ForceField::Notify( class UAnimNodeSequence* NodeSeq )
{
	if( ForceFieldComponent == NULL )
	{
		return;
	}
	USkeletalMeshComponent* SkeletalMeshComponent = NodeSeq->SkelComponent;
	UBOOL 	bValidSocket, bValidBone;
	// See if the bone name refers to an existing socket on the skeletal mesh.
	bValidSocket	= (SkeletalMeshComponent->SkeletalMesh->FindSocket(SocketName) != NULL);
	bValidBone		= (SkeletalMeshComponent->MatchRefBone(BoneName) != INDEX_NONE);
	if( SkeletalMeshComponent )
	{
		if(bAttach)
		{
			if (!(bValidBone || bValidSocket))
			{
				// Nothing to Attach to
				return;
			}

			ASkeletalMeshActor* Owner = Cast<ASkeletalMeshActor>(SkeletalMeshComponent->GetOwner());
			// Attach Component to SkeletalMeshActor
			if ( Owner )
			{
				Owner->eventCreateForceField(this);
			}
			// Attach Component to SkeletalMeshComponent
			else
			{
				SkeletalMeshComponent->eventCreateForceField(this);
			}
		}
		else
		{
			// Find Spawning Location. First using the socket, bone, the Actor that the SkeletalMeshComponent is attached to, SkeletalMeshComponent. 
			FVector Location;
			FRotator Rotation(0, 0, 1);
			AActor* Owner = SkeletalMeshComponent->GetOwner();
			if (bValidSocket)
			{
				SkeletalMeshComponent->GetSocketWorldLocationAndRotation(SocketName, Location, &Rotation);
			}
			else if (bValidBone)
			{
				Location = SkeletalMeshComponent->GetBoneLocation(BoneName);
			}
			else if (Owner)
			{
				Location = Owner->Location;
				Rotation = Owner->Rotation;
			}
			else
			{
				Location = SkeletalMeshComponent->Translation;
				Rotation = SkeletalMeshComponent->Rotation;
			}

			// Spawn Actor and Add the ForceField Component to it
			ANxForceFieldSpawnable* SpawnedActor = Cast<ANxForceFieldSpawnable>(GWorld->SpawnActor(ANxForceFieldSpawnable::StaticClass(), NAME_None, Location, Rotation ));
			if (SpawnedActor)
			{
				SpawnedActor->ForceFieldComponent = Cast<UNxForceFieldComponent>(
					UObject::StaticDuplicateObject(
					ForceFieldComponent,
					ForceFieldComponent,
					SpawnedActor,
					TEXT("None")
					)
					);
				SpawnedActor->Components.AddItem(SpawnedActor->ForceFieldComponent);
				SpawnedActor->ForceFieldComponent->DoInitRBPhys();
			}
		}

	}
}

IMPLEMENT_CLASS(UAnimNotify_ForceField);

#if !FINAL_RELEASE

void GatherAnimSequenceStats(FOutputDevice& Ar)
{
	INT AnimationKeyFormatNum[AKF_MAX];
	INT TranslationCompressionFormatNum[ACF_MAX];
	INT RotationCompressionFormatNum[ACF_MAX];
	appMemzero( AnimationKeyFormatNum, AKF_MAX * sizeof(INT) );
	appMemzero( TranslationCompressionFormatNum, ACF_MAX * sizeof(INT) );
	appMemzero( RotationCompressionFormatNum, ACF_MAX * sizeof(INT) );

	Ar.Logf( TEXT(" %60s, Frames,NTT,NRT, NT1,NR1, TotTrnKys,TotRotKys,Codec,ResBytes"), TEXT("Sequence Name") );
	INT GlobalNumTransTracks = 0;
	INT GlobalNumRotTracks = 0;
	INT GlobalNumTransTracksWithOneKey = 0;
	INT GlobalNumRotTracksWithOneKey = 0;
	INT GlobalApproxCompressedSize = 0;
	INT GlobalApproxKeyDataSize = 0;
	INT GlobalNumTransKeys = 0;
	INT GlobalNumRotKeys = 0;

	for( TObjectIterator<UAnimSequence> It; It; ++It )
	{
		UAnimSequence* Seq = *It;

		INT NumTransTracks = 0;
		INT NumRotTracks = 0;
		INT TotalNumTransKeys = 0;
		INT TotalNumRotKeys = 0;
		FLOAT TranslationKeySize = 0.0f;
		FLOAT RotationKeySize = 0.0f;
		INT OverheadSize = 0;
		INT NumTransTracksWithOneKey = 0;
		INT NumRotTracksWithOneKey = 0;

		AnimationFormat_GetStats(
			Seq, 
			NumTransTracks,
			NumRotTracks,
			TotalNumTransKeys,
			TotalNumRotKeys,
			TranslationKeySize,
			RotationKeySize,
			OverheadSize,
			NumTransTracksWithOneKey,
			NumRotTracksWithOneKey);

		GlobalNumTransTracks += NumTransTracks;
		GlobalNumRotTracks += NumRotTracks;
		GlobalNumTransTracksWithOneKey += NumTransTracksWithOneKey;
		GlobalNumRotTracksWithOneKey += NumRotTracksWithOneKey;

		GlobalApproxCompressedSize += Seq->GetApproxCompressedSize();
		GlobalApproxKeyDataSize += (INT)((TotalNumTransKeys * TranslationKeySize) + (TotalNumRotKeys * RotationKeySize));

		GlobalNumTransKeys += TotalNumTransKeys;
		GlobalNumRotKeys += TotalNumRotKeys;

		Ar.Logf(TEXT(" %60s, %3i, %3i,%3i, %3i,%3i, %10i,%10i, %s, %i"),
			*Seq->SequenceName.ToString(),
			Seq->NumFrames,
			NumTransTracks, NumRotTracks,
			NumTransTracksWithOneKey, NumRotTracksWithOneKey,
			TotalNumTransKeys, TotalNumRotKeys,
			*FAnimationUtils::GetAnimationKeyFormatString(static_cast<AnimationKeyFormat>(Seq->KeyEncodingFormat)),
			Seq->GetResourceSize() );
	}
	Ar.Logf( TEXT("======================================================================") );
	Ar.Logf( TEXT("Total Num Tracks: %i trans, %i rot, %i trans1, %i rot1"), GlobalNumTransTracks, GlobalNumRotTracks, GlobalNumTransTracksWithOneKey, GlobalNumRotTracksWithOneKey );
	Ar.Logf( TEXT("Total Num Keys: %i trans, %i rot"), GlobalNumTransKeys, GlobalNumRotKeys );

	Ar.Logf( TEXT("Approx Compressed Memory: %i bytes"), GlobalApproxCompressedSize);
	Ar.Logf( TEXT("Approx Key Data Memory: %i bytes"), GlobalApproxKeyDataSize);
}

#endif

/************************************************************************************
 * UAnimMetaData
 ***********************************************************************************/

void UAnimMetaData::AnimSet(UAnimNodeSequence* SeqNode)
{
}

void UAnimMetaData::AnimUnSet(UAnimNodeSequence* SeqNode)
{
}

void UAnimMetaData::TickMetaData(UAnimNodeSequence* SeqNode)
{
}

/************************************************************************************
 * UAnimMetaData_SkelControl
 ***********************************************************************************/

void UAnimMetaData_SkelControl::PostLoad()
{
	UBOOL bMarkDirty = FALSE;
	Super::PostLoad();

	// Fixup switch from single SkelControlName to Array of names.
	if( GetLinkerVersion() < VER_SKELCONTROL_ANIMMETADATA_LIST )
	{
		SkelControlNameList.AddItem(SkelControlName_DEPRECATED);
		bMarkDirty = TRUE;
	}

	if( bMarkDirty && (GIsRunning || GIsUCC) )
	{
		MarkPackageDirty();
	}
}

void UAnimMetaData_SkelControl::AnimSet(UAnimNodeSequence* SeqNode)
{
	Super::AnimSet(SeqNode);

	for(INT NameIdx=0; NameIdx<SkelControlNameList.Num(); NameIdx++)
	{
		if( SkelControlNameList(NameIdx) != NAME_None )
		{
			// Find SkelControl, and add us to the list of cached AnimNodeSequences
			USkelControlBase* SkelControl = SeqNode->SkelComponent->FindSkelControl( SkelControlNameList(NameIdx) );
			if( SkelControl )
			{
				SeqNode->MetaDataSkelControlList.AddUniqueItem(SkelControl);
			}
		}
	}
}

void UAnimMetaData_SkelControl::AnimUnSet(UAnimNodeSequence* SeqNode)
{
	Super::AnimUnSet(SeqNode);

	if ( SeqNode->SkelComponent )
	{
		for(INT NameIdx=0; NameIdx<SkelControlNameList.Num(); NameIdx++)
		{
			if( SkelControlNameList(NameIdx) != NAME_None )
			{
				// Find SkelControl, and remove us from the list of cached AnimNodeSequences
				USkelControlBase* SkelControl = SeqNode->SkelComponent->FindSkelControl( SkelControlNameList(NameIdx) );
				if( SkelControl )
				{
					SeqNode->MetaDataSkelControlList.RemoveItem(SkelControl);
				}
			}
		}
	}
}

void UAnimMetaData_SkelControl::TickMetaData(UAnimNodeSequence* SeqNode)
{
	for(INT SkelControlIdx=0; SkelControlIdx<SeqNode->MetaDataSkelControlList.Num(); SkelControlIdx++)
	{
		USkelControlBase* SkelControl = SeqNode->MetaDataSkelControlList(SkelControlIdx);
		if( ShouldCallSkelControlTick(SkelControl, SeqNode) )
		{
			// Reset weights once  per frame.
			if( SkelControl->AnimMetaDataUpdateTag != SeqNode->NodeTickTag )
			{
				SkelControl->AnimMetaDataUpdateTag = SeqNode->NodeTickTag;
				SkelControl->AnimMetadataWeight = 0.f;
			}

			SkelControlTick(SkelControl, SeqNode);
		}
	}
}

/** Only call the tick function for the right SkelControl. */
UBOOL UAnimMetaData_SkelControl::ShouldCallSkelControlTick(USkelControlBase* SkelControl, UAnimNodeSequence* SeqNode)
{
	if( !bFullControlOverController || SkelControl->bControlledByAnimMetada )
	{
		for(INT NameIdx=0; NameIdx<SkelControlNameList.Num(); NameIdx++)
		{
			if( SkelControlNameList(NameIdx) == SkelControl->ControlName )
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

/** Increase AnimMetaDataWeight by our AnimNodeSequence's total weight contribution in the Tree. */
void UAnimMetaData_SkelControl::SkelControlTick(USkelControlBase* SkelControl, UAnimNodeSequence* SeqNode)
{
	if( bFullControlOverController )
	{
		// Add up this node's total weight contribution to the tree.
		// No need to clamp AnimMetadataWeight, that is done for us in the SkelControl.
		SkelControl->AnimMetadataWeight = ::Min(SkelControl->AnimMetadataWeight + SeqNode->NodeTotalWeight, 1.f);
	}
}

/************************************************************************************
 * UAnimMetaData_SkelControlKeyFrame
 ***********************************************************************************/

/** Increase AnimMetaDataWeight by our AnimNodeSequence's total weight contribution in the Tree multiplied by the key framed weight, user setup. */
void UAnimMetaData_SkelControlKeyFrame::SkelControlTick(USkelControlBase* SkelControl, UAnimNodeSequence* SeqNode)
{
	// Set default start time
	FLOAT StartTime = 0.0f, EndTime = -1.f;
	FLOAT StartStrength = 0.f, EndStrength = 0.f;

	// need to find where Current Time fits
	for (INT KeyIndex=0; KeyIndex<KeyFrames.Num(); KeyIndex++)
	{
		FTimeModifier& Key = KeyFrames(KeyIndex);
		if( Key.Time > SeqNode->CurrentTime )
		{
			EndTime = Key.Time;		
			EndStrength = Key.TargetStrength;
			break;
		}
		else
		{
			StartTime = Key.Time;
			StartStrength = Key.TargetStrength;
		}
	}

	// End time isn't set because current time is after last index
	// Set with total length, and start strength; 
	if( EndTime < 0.f )
	{
		EndTime = SeqNode->AnimSeq->SequenceLength;
		EndStrength = StartStrength;
	}

	// linear blend
	FLOAT const KeyFrameWeight = StartStrength + ((SeqNode->CurrentTime-StartTime)/(EndTime-StartTime))*(EndStrength-StartStrength);
	if( bFullControlOverController )
	{
		// If we take full control of the node, then we modulate by node's weight (for blend transitions)
		// and work on AnimMetadataWeight.
		SkelControl->AnimMetadataWeight = ::Min(SkelControl->AnimMetadataWeight + SeqNode->NodeTotalWeight * KeyFrameWeight, 1.f);
	}
	else
	{
		// Otherwise, we just overwrite the control's strength with our weight.
		// It's a simple way to override and set the weight of the bone controller.
		SkelControl->ControlStrength = KeyFrameWeight;
	}
}

/**
 *************************************************
 * HeadTrackingComponent for skeletalmeshcomponent
 *************************************************
 * This is to collect actors/properly find best candidate to look at
 * Used by Matinee/Kismet by LDs set up
 * Can be used for any kind of skeletalmeshcomponet as far as the animtree contains correct look at skelcontrol
 * When you attach this class, make sure you don't have any other HeadTrackingComponent 
 * That will create conflict. It will warn if it already has headtrackingcomponent
 */

/** Enable/Disable HeadTracking **/
void UHeadTrackingComponent::EnableHeadTracking(UBOOL bEnable)
{
	if ( bEnable )
	{
		// need to clear this up
		for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
		{
			FActorToLookAt * ActorToLookAt = It.Value();
			delete It.Value();
		}

		CurrentActorMap.Empty();

		TrackControls.Empty();
		// find new track controls
		RefreshTrackControls();
	}
	else
	{
		// need to clear this up
		for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
		{
			FActorToLookAt * ActorToLookAt = It.Value();
			delete It.Value();
		}

		CurrentActorMap.Empty();

		for (INT I=0; I<TrackControls.Num(); ++I)
		{
			TrackControls(I)->SetSkelControlStrength(0.f, 0.25f);
		}

		TrackControls.Empty();
	}
}

/**
 * Attaches the component to a ParentToWorld transform, owner and scene.
 * Requires IsValidComponent() == true.
 */
void UHeadTrackingComponent::Attach()
{	
	Super::Attach();

	// if my owner has another headtrackingcomponent, warn it
	AActor * MyOwner = GetOwner();
	if ( MyOwner )
	{
		// only if actorcomponent
		for (INT ComponentIndex = 0; ComponentIndex < MyOwner->Components.Num(); ComponentIndex++)
		{
			// if this isn't me
			if (MyOwner->Components(ComponentIndex) != this)
			{
				UClass * ComponentClass = MyOwner->Components(ComponentIndex)->GetClass();
				if ( ComponentClass->IsChildOf(UHeadTrackingComponent::StaticClass()) )
				{
					// another head tracking component is already attached
					debugf(TEXT("%s already has HeadTracking Component. Adding multiple headtracking components won't work."), *MyOwner->GetName());
				}
			}
		}
	}
}

/**
 * Detaches the component from the scene it is in.
 * Requires bAttached == true
 *
 * @param bWillReattach TRUE is passed if Attach will be called immediately afterwards.  This can be used to
 *                      preserve state between reattachments.
 */
void UHeadTrackingComponent::Detach( UBOOL bWillReattach )
{
	// disable head track control
	// unless it's getting reattached
	if (!bWillReattach)
	{
		EnableHeadTracking(FALSE);
	}

	Super::Detach(bWillReattach);
}

/**
 * Updates time dependent state for this component.
 * Requires bAttached == true.
 * @param DeltaTime - The time since the last tick.
 */
void UHeadTrackingComponent::Tick(FLOAT DeltaTime)
{
	UpdateHeadTracking(DeltaTime);
	Super::Tick(DeltaTime);
}

#define DEBUG_HEADTRACKING 0

/** Get Pawn from the given Actor **/
APawn * GetPawn(AActor *Actor)
{
	if (Actor)
	{
		APawn * Pawn = Actor->GetAPawn();
		if (!Pawn)
		{
			// I can't do playercontroller since UT uses UTPlayerController to animate camera
			// For now we only support AI
			if (Actor->IsA(AController::StaticClass()))
			{
				Pawn = CastChecked<AController>(Actor)->Pawn;
			}
		}

		return Pawn;
	}

	return NULL;
}

/** Get SkeletalMeshComp from the given Actor **/
USkeletalMeshComponent * GetSkeletalMeshComp( AActor * Actor )
{
	USkeletalMeshComponent * SkeletalMeshComp=NULL;
	APawn * Pawn = GetPawn(Actor);

	if (Pawn)
	{
		SkeletalMeshComp = Pawn->Mesh;
	}
	else
	{
		// FIXME: should I change to SkeletalMeshActor?
		ASkeletalMeshActorMAT * MATActor = Cast<ASkeletalMeshActorMAT>(Actor);

		if (MATActor)
		{
			SkeletalMeshComp = MATActor->SkeletalMeshComponent;
		}
		else
		{
			debugf(TEXT("Unknown actor for head tracking track. Only support AI or SkeletalMeshActorMAT"));
		}
	}

	return  SkeletalMeshComp;
}

/** 
 * Update Acotr Map
 */
INT UHeadTrackingComponent::UpdateActorMap(FLOAT CurrentTime)
{
	if ( TrackControls.Num() > 0  && SkeletalMeshComp && SkeletalMeshComp->GetOwner() )
	{
		// find where is my position/rotation
		AActor * Owner = SkeletalMeshComp->GetOwner();
		RootMeshLocation = Owner->Location;
		RootMeshRotation = Owner->Rotation;
		// we consider first one as base, and will calculate based on first one to target
		USkelControlLookAt * LookAtControl = TrackControls(0);
		if ( LookAtControl && LookAtControl->ControlBoneIndex!=INDEX_NONE )
		{
			// change MeshLocation to be that bone index
			FBoneAtom MeshRootBA = SkeletalMeshComp->GetBoneAtom(LookAtControl->ControlBoneIndex);
			RootMeshLocation = MeshRootBA.GetOrigin();
			if (SkeletalMeshComp->SkeletalMesh)
			{
				// apply local mesh transform to the baselookdir
				FMatrix RotMatrix = FRotationMatrix(SkeletalMeshComp->SkeletalMesh->RotOrigin);
				RootMeshRotation = RotMatrix.TransformNormal(LookAtControl->BaseLookDir).Rotation();
			}
			else
			{
				RootMeshRotation = LookAtControl->BaseLookDir.Rotation();
			}
		}

		// find actors around me
		FMemMark Mark( GMainThreadMemStack );
		FCheckResult* Link=GWorld->Hash->ActorRadiusCheck( GMainThreadMemStack, RootMeshLocation, LookAtActorRadius, TRACE_Actors );
		TArray<AActor *> ActorList;
		while ( Link )
		{
			if( Link->Actor &&
				Link->Actor->bCollideActors && 
				!Link->Actor->bDeleteMe )
			{
				// go through actor list to see if I have it. 
				for ( INT ActorID =0; ActorID < ActorClassesToLookAt.Num(); ++ActorID )
				{
					if (Link->Actor->IsA( ActorClassesToLookAt(ActorID) ))
					{
						ActorList.AddUniqueItem(Link->Actor);
						break;
					}
				}
			
				Link = Link->GetNext();
			}
		}

		// add new items to the map
		for ( INT ActorID = 0; ActorID < ActorList.Num(); ++ActorID )
		{
			FActorToLookAt* ActorToLookAt = NULL;

			// if it's not in the list yet add
			if ( CurrentActorMap.HasKey(ActorList(ActorID)) == FALSE )
			{
				ActorToLookAt = new FActorToLookAt;
				ActorToLookAt->Actor = ActorList(ActorID);
				ActorToLookAt->EnteredTime = CurrentTime;
				ActorToLookAt->CurrentlyBeingLookedAt = FALSE;
				ActorToLookAt->LastKnownDistance = 0.f;
				ActorToLookAt->StartTimeBeingLookedAt = 0.f;
				ActorToLookAt->Rating = 0.f;
				CurrentActorMap.Set(ActorToLookAt->Actor, ActorToLookAt);
			}
		}

		return CurrentActorMap.Num();
	}

	return 0;
}
/**
 * Find Best Candidate from the current listing
*/
FActorToLookAt * UHeadTrackingComponent::FindBestCandidate(FLOAT CurrentTime)
{
	// now run ratings
	FLOAT  LookAtActorRadiusSq =  LookAtActorRadius * LookAtActorRadius;
	FActorToLookAt * BestCandidate = NULL;
	FLOAT	BestRating = -99999.f;

	// now update their information
	for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
	{
		FActorToLookAt * ActorToLookAt = It.Value();
		ActorToLookAt->LastKnownDistance = (RootMeshLocation-ActorToLookAt->Actor->Location).SizeSquared();
		// outside of raius, do not care, delete them
		if (ActorToLookAt->LastKnownDistance > LookAtActorRadiusSq)
		{
			delete It.Value();
			It.RemoveCurrent();
		}
		else
		{
			// update rating
			// if closer, higher rating - 1 for distance, 1 for recently entered
			FLOAT DistanceRating = 1 - ActorToLookAt->LastKnownDistance/LookAtActorRadiusSq;
			// clamp time rating. Otherwise, you're never going to get second chance
			FLOAT TimeRating = Max(-1.f, (MaxInterestTime - (CurrentTime-ActorToLookAt->EnteredTime))/MaxInterestTime);
			FLOAT LookAtRating = 0.f;
			FLOAT LookAtTime = MinLookAtTime + appFrand()*(MaxLookAtTime-MinLookAtTime);

			if (ActorToLookAt->CurrentlyBeingLookedAt)
			{
				// if less than 1 second, give boost, don't like to switch every time
				LookAtRating = (LookAtTime - (CurrentTime-ActorToLookAt->StartTimeBeingLookedAt))/LookAtTime;
			}
			else if (CurrentTime-ActorToLookAt->StartTimeBeingLookedAt < LookAtTime*2.f)
			{
				// if he has been looked at before, 
				LookAtRating = (LookAtTime - (CurrentTime-ActorToLookAt->StartTimeBeingLookedAt))/LookAtTime;
			}
			else
			{
				// first time? Give boost
				LookAtRating = 0.8f;
			}

			// if it's in front of me, have more rating
			FLOAT AngleRating = (ActorToLookAt->Actor->Location-RootMeshLocation).SafeNormal() | RootMeshRotation.Vector();
			// give boost if target is moving. More interesting to see. 
			FLOAT MovingRating = (ActorToLookAt->Actor->Velocity.IsZero())? 0.f : 1.0f;

			ActorToLookAt->Rating = DistanceRating + TimeRating + LookAtRating + AngleRating + MovingRating;
#if DEBUG_HEADTRACKING
			debugf(TEXT("HeadTracking: [%s] Ratings(%0.2f), DistanceRating(%0.2f), TimeRaiting(%0.2f), LookAtRating(%0.2f), AngleRating(%0.2f), Moving Rating(%0.2f), CurrentLookAtTime(%0.2f)"), *ActorToLookAt->Actor->GetName(), ActorToLookAt->Rating, DistanceRating, TimeRating, LookAtRating, AngleRating, MovingRating, LookAtTime );
#endif
			if ( ActorToLookAt->Rating > BestRating && ActorToLookAt->Actor )
			{
				BestRating = ActorToLookAt->Rating;
				BestCandidate = ActorToLookAt;
			}
		}
	}

	return BestCandidate;
}
/**
 *  Update Head Tracking
 */
void UHeadTrackingComponent::UpdateHeadTracking(FLOAT DeltaTime)
{
	FLOAT CurrentTime = GWorld->GetTimeSeconds();

	UpdateActorMap(CurrentTime);

	FActorToLookAt * BestCandidate = FindBestCandidate(CurrentTime);

	if (BestCandidate)
	{
		for (INT I=0; I<TrackControls.Num(); ++I)
		{
			TrackControls(I)->SetSkelControlStrength(1.f, 0.25f);
		}

#if DEBUG_HEADTRACKING
		debugf(TEXT("HeadTracking: Best Candidate [%s] "), *BestCandidate->Actor->GetName());
#endif
		if (BestCandidate->CurrentlyBeingLookedAt==FALSE)
		{
			BestCandidate->StartTimeBeingLookedAt = CurrentTime;
			for (INT I=0; I<TrackControls.Num(); ++I)
			{
				TrackControls(I)->SetLookAtAlpha(1.0f, 0.25f);
			}
		}

		BestCandidate->CurrentlyBeingLookedAt = TRUE;

		FVector TargetLoc = (BestCandidate->Actor->Location);
		if (TargetBoneNames.Num())
		{
			USkeletalMeshComponent * MeshComp = GetSkeletalMeshComp(BestCandidate->Actor);
			if (MeshComp)
			{
				for (INT TargetID=0; TargetID < TargetBoneNames.Num(); ++TargetID)
				{
					INT BoneIdx = MeshComp->MatchRefBone(TargetBoneNames(TargetID));
					if (BoneIdx != INDEX_NONE)
					{
						// found it, get out
						TargetLoc = MeshComp->GetBoneAtom(BoneIdx).GetOrigin();
						break;
					}
				}
			}
		}

		for (INT I=0; I<TrackControls.Num(); ++I)
		{
			TrackControls(I)->DesiredTargetLocation = TargetLoc;
			TrackControls(I)->InterpolateTargetLocation(DeltaTime);
		}

#if DEBUG_HEADTRACKING
		GWorld->GetWorldInfo()->FlushPersistentDebugLines();

		GWorld->GetWorldInfo()->DrawDebugCoordinateSystem(RootMeshLocation, RootMeshRotation, 10, TRUE);
		GWorld->GetWorldInfo()->DrawDebugLine(RootMeshLocation, BestCandidate->Actor->Location, 0, 0, 255, TRUE);
		GWorld->GetWorldInfo()->DrawDebugLine(RootMeshLocation, TrackControls(0)->DesiredTargetLocation, 255, 0, 0, TRUE);
		GWorld->GetWorldInfo()->DrawDebugLine(RootMeshLocation, TrackControls(0)->TargetLocation, 255, 255, 0, TRUE);
#endif
		for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
		{
			FActorToLookAt * ActorToLookAt = It.Value();
			if (ActorToLookAt != BestCandidate)
			{
				ActorToLookAt->CurrentlyBeingLookedAt = FALSE;
			}
		}
	}
	else
	{
		// if nothing else turn that off
		if (TrackControls.Num() > 0 )
		{
#if DEBUG_HEADTRACKING
			debugf(TEXT("HeadTracking: Turning it off "));
#endif
			for (INT I=0; I<TrackControls.Num(); ++I)
			{
				TrackControls(I)->SetSkelControlStrength(0.f, 0.25f);
			}
		}
	}
}

/** 
 * Refresh Head Tracking Skel Control List
 */
void UHeadTrackingComponent::RefreshTrackControls()
{
	// if head track controls are not found yet
	if (SkeletalMeshComp && TrackControls.Num() == 0 && TrackControllerName.Num() > 0 )
	{
		if ( SkeletalMeshComp && SkeletalMeshComp->SkeletalMesh && SkeletalMeshComp->Animations && SkeletalMeshComp->Animations->IsA(UAnimTree::StaticClass()) )
		{
			//now look for look at control
			UAnimTree * AnimTree = CastChecked<UAnimTree>(SkeletalMeshComp->Animations);

			if ( AnimTree )
			{
				for (INT I=0; I<TrackControllerName.Num(); ++I)
				{
					USkelControlLookAt* LookAtControl = Cast<USkelControlLookAt>(AnimTree->FindSkelControl(TrackControllerName(I)));
					if (LookAtControl)
					{
						TrackControls.AddItem(LookAtControl);
					}
				}
			}
		}

		if (TrackControls.Num() > 0)
		{
			for (INT I=0; I<TrackControls.Num(); ++I)
			{
				TrackControls(I)->bDisableBeyondLimit = bDisableBeyondLimit;
				// initialize as turn off
				TrackControls(I)->SetSkelControlStrength(0.f, 0.25f);
			}
		}
		else
		{
			debugf(TEXT("Track control not found for mesh [%s]."), *SkeletalMeshComp->SkeletalMesh->GetName() );
		}
	}
}

/** Clear list **/
void UHeadTrackingComponent::BeginDestroy()
{
	// need to clear this up before destoying
	// just in case it hasn't been detached
	for( TMap<class AActor*,struct FActorToLookAt*>::TIterator It(CurrentActorMap); It; ++It )
	{
		FActorToLookAt * ActorToLookAt = It.Value();
		delete It.Value();
	}

	CurrentActorMap.Empty();

	Super::BeginDestroy();
}
