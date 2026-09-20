/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Teleport extends SequenceAction;

/** If true, actor rotation will be aligned with destination actor */
var() bool bUpdateRotation;
// BM
var() bool bDontResetCamera;
var() bool bDontResetState;
var() bool bSnapPlayerAnim;
var() bool bStopAllMovement;
var() bool bSpawnBatmobile;
var() bool bForceResetBatmobileRigidBody;
var() bool bForcePawnIntoCrouch;
var() bool ForceAddStreamingLevelOffset;
var() bool UseDestinationCharacterOrVehicleBottom;
/** Rotation added on top of the destination's rotation */
var() Rotator RotationOffset;

/** @return Whether the given Actor should be teleported */
final static function bool ShouldTeleport(Actor TestActor, vector TeleportLocation, optional float TeleportDist, optional array<Volume> Volumes )
{
	local int VolumeIdx;

	if (TeleportDist > 0.0 && VSizeSq(TestActor.Location - TeleportLocation) < TeleportDist*TeleportDist)
	{
		return false;
	}
	else if (Volumes.length > 0)
	{
		for (VolumeIdx = 0; VolumeIdx < Volumes.length; ++VolumeIdx)
		{
			if (Volumes[VolumeIdx] != None && Volumes[VolumeIdx].Encompasses(TestActor))
			{
				return false;
			}
		}
	}

	return true;
}

defaultproperties
{
	ObjName="Teleport"
	ObjCategory="Actor"
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Destination")
	bUpdateRotation=TRUE
}
