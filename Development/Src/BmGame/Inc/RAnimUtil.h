/*=============================================================================
	RAnimUtil.h
	BM: Shared animation helpers.
=============================================================================*/

#ifndef __RANIMUTIL_H__
#define __RANIMUTIL_H__

FMatrix GetLookAtMatrix(class USkeletalMeshComponent* SkelComponent, INT BoneIndex);
FLOAT GetYaw(FLOAT X, FLOAT Y);
FLOAT GetPitch(FLOAT X, FLOAT Y, FLOAT Z);
void GetLookAtFaceFXRegisters(FLOAT Yaw, FLOAT Pitch, UBOOL bAllowSensibleClamping, FLOAT& Out_Yaw, FLOAT& Out_Pitch);
UBOOL GetLookAtRegisters(class AActor* Actor, class AActor* TargetActor, FLOAT Weight, FLOAT& Out_Yaw, FLOAT& Out_Pitch);

#endif
