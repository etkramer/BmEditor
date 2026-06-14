/**
 * Base class for static actors which contain StaticMeshComponents.
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class StaticMeshActorBase extends Actor
	ClassGroup(StaticMeshes)
	native
	abstract;

var(Collision) const bool bRailing;
var(Collision) const bool bSpikeyRailing;
var(Collision) const bool bUseBoundingBoxForClimbing;
var(Collision) const bool bClimbableSlopedRailing;
var(Collision) const bool bDontAdjustCameraForSlope;
var(Collision) const bool bNeverUseBracedShimmy;
var(Advanced) const bool bAllowWideRailings;

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
