/*=============================================================================
	RAnimUtil.cpp
	BM: Shared animation helpers.
=============================================================================*/

#include "BmGame.h"

/** Largest register value a clamped look-at is allowed to produce. */
static const FLOAT SensibleLimit = 8.625f;

/** Head look-at basis for a bone: X is the bone's -Y, Y is the bone's -Z, Z is the bone's X. */
FMatrix GetLookAtMatrix(USkeletalMeshComponent* SkelComponent, INT BoneIndex)
{
	check(BoneIndex != INDEX_NONE);

	const FMatrix BoneToWorld = SkelComponent->SpaceBases(BoneIndex).ToMatrix() * SkelComponent->LocalToWorld;

	FMatrix Result;
	for( INT i=0; i<3; i++ )
	{
		Result.M[0][i] = -BoneToWorld.M[1][i];
		Result.M[1][i] = -BoneToWorld.M[2][i];
		Result.M[2][i] =  BoneToWorld.M[0][i];
		Result.M[3][i] =  BoneToWorld.M[3][i];
	}
	Result.M[0][3] = 0.f;
	Result.M[1][3] = 0.f;
	Result.M[2][3] = 0.f;
	Result.M[3][3] = 1.f;

	return Result;
}

FLOAT GetYaw(FLOAT X, FLOAT Y)
{
	return appAtan2(Y, X);
}

FLOAT GetPitch(FLOAT X, FLOAT Y, FLOAT Z)
{
	const FLOAT Length = appSqrt(X*X + Y*Y + Z*Z);
	if( Length <= 0.f )
	{
		return 0.f;
	}

	return appAsin( Clamp(Z / Length, -1.f, 1.f) );
}

/** Convert a look-at angle pair into the FaceFX register values that drive the head. */
void GetLookAtFaceFXRegisters(FLOAT Yaw, FLOAT Pitch, UBOOL bAllowSensibleClamping, FLOAT& Out_Yaw, FLOAT& Out_Pitch)
{
	Out_Yaw = Yaw * 13.773025f;
	Out_Pitch = Pitch * ((Pitch < 0.f) ? -29.382452f : -40.925556f);

	if( bAllowSensibleClamping )
	{
		Out_Yaw = Clamp(Out_Yaw, -SensibleLimit, SensibleLimit);
		Out_Pitch = Clamp(Out_Pitch, -SensibleLimit, SensibleLimit);
	}
}

/** Work out the FaceFX look-at register values that point Actor's head at TargetActor. */
UBOOL GetLookAtRegisters(AActor* Actor, AActor* TargetActor, FLOAT Weight, FLOAT& Out_Yaw, FLOAT& Out_Pitch)
{
	if( !Actor || !TargetActor )
	{
		return FALSE;
	}

	ASkeletalMeshActor* SkelActor = Cast<ASkeletalMeshActor>(Actor);
	if( !SkelActor )
	{
		warnf(NAME_Warning, TEXT("Expected SkeletalMeshActor, got %s."), *Actor->GetName());
		return FALSE;
	}

	// BM: the game drives the proxied actor's head mesh instead when this is a proxying RCinematicActor.
	USkeletalMeshComponent* SkelComponent = SkelActor->SkeletalMeshComponent;
	if( !SkelComponent )
	{
		return FALSE;
	}

	static const FName NAME_Bip01_Head(TEXT("Bip01_Head"));
	const INT BoneIndex = SkelComponent->MatchRefBone(NAME_Bip01_Head);
	if( BoneIndex == INDEX_NONE || BoneIndex >= SkelComponent->SpaceBases.Num() )
	{
		return FALSE;
	}

	const FMatrix InvLookAt = GetLookAtMatrix(SkelComponent, BoneIndex).Inverse();
	const FVector LocalTarget = InvLookAt.TransformFVector(TargetActor->Location);
	if( LocalTarget.SizeSquared() < 0.0001f )
	{
		return FALSE;
	}

	const FLOAT Yaw = GetYaw(LocalTarget.X, LocalTarget.Y);
	const FLOAT Pitch = GetPitch(LocalTarget.X, LocalTarget.Y, LocalTarget.Z);
	GetLookAtFaceFXRegisters(Yaw, Pitch, FALSE, Out_Yaw, Out_Pitch);

	Out_Yaw *= Weight;
	Out_Pitch *= Weight;

	return TRUE;
}
