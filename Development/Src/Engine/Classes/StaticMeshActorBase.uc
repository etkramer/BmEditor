/**
 * Base class for static actors which contain StaticMeshComponents.
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class StaticMeshActorBase extends RStaticClimbableActor
	ClassGroup(StaticMeshes)
	native
	abstract;

var(Collision) const bool bDontAdjustCameraForSlope;
var(Vehicle) const bool bIsVehicleBigJump;
var(Vehicle) const bool bVehicleJumpAutoAlignX;
var(Vehicle) const bool bVehicleJumpAutoAlignY;
var(Vehicle) const bool bVehicleJumpAutoAlignNegX;
var(Vehicle) const bool bVehicleJumpAutoAlignNegY;
var(Advanced) const bool bHideIfFlexEnabled;
var transient notforconsole bool bWithinPxSublevel;

cpptext
{
	/**
	 * Initializes this actor when play begins.  This version marks the actor as ready to execute script, but skips
	 * the rest of the stuff that actors normally do in PostBeginPlay().
	 */
	virtual void PostBeginPlay();
}

DefaultProperties
{
	bEdShouldSnap=true
	bStatic=true
	bMovable=false
	bCollideActors=true
	bBlockActors=true
	bWorldGeometry=true
	bGameRelevant=true
	bRouteBeginPlayEvenIfStatic=false
	bCollideWhenPlacing=false
}
