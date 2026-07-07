/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Teleport extends SequenceAction;

/** If true, actor rotation will be aligned with destination actor */
var() bool bUpdateRotation;
/** If actor is more than this far away, it will be teleported. Ignored if < 0 */
var() float TeleportDistance;
/** If actor is NOT in one of these volumes, it will be teleported */
var() array<Volume> TeleportVolumes;

/** If TRUE, check to see if this actor overlaps any other colliding actors and don't teleport there if a better option exists */
var() bool bCheckOverlap;

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
	VariableLinks(2)=(ExpectedType=class'SeqVar_Object',LinkDesc="Teleport Volumes",PropertyName=TeleportVolumes,bHidden=TRUE)
	bUpdateRotation=TRUE

	TeleportDistance=-1.f
}
