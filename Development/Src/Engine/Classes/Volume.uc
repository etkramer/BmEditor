//=============================================================================
// Volume:  a bounding volume
// touch() and untouch() notifications to the volume as actors enter or leave it
// enteredvolume() and leftvolume() notifications when center of actor enters the volume
// pawns with bIsPlayer==true  cause playerenteredvolume notifications instead of actorenteredvolume()
// Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class Volume extends Brush
	native
	nativereplication;

/** Should pawns be forced to walk when inside this volume? */
var() bool bForcePawnWalk;

/** Should process all actors within this volume */
var() bool bProcessAllActors;

var() bool bOnlyCollideWithPlayer;
var() bool bNoWallPlant;
var() PhysicalMaterial PhysicalMaterialOverrideForCollisionComponent;

cpptext
{
	INT Encompasses(FVector point);
	void SetVolumes();
	virtual void SetVolumes(const TArray<class AVolume*>& Volumes);
	virtual UBOOL ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags);
	virtual UBOOL IsAVolume() const {return TRUE;}
	virtual AVolume* GetAVolume() { return this; }
	virtual INT* GetOptimizedRepList(BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel);
	virtual void PostEditImport();

#if WITH_EDITOR
	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
	virtual void CheckForErrors();
#endif
}

native noexport function bool Encompasses(Actor Other); // returns true if center of actor is within volume
native noexport function bool EncompassesPoint( Vector Loc );

/**
 * list important Volume variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when
 * the ShowDebug exec is used
 *
 * @param	HUD		- HUD with canvas to draw on
 * @input	out_YL		- Height of the current font
 * @input	out_YPos	- Y position on Canvas. out_YPos += out_YL, gives position to draw text for next debug line.
 */
simulated function DisplayDebug(HUD HUD, out float out_YL, out float out_YPos)
{
	super.DisplayDebug(HUD, out_YL, out_YPos);
}

/**	Handling Toggle event from Kismet. */
simulated function OnToggle(SeqAct_Toggle Action)
{
	// Turn ON
	if (Action.InputLinks[0].bHasImpulse)
	{
		if(!bCollideActors)
		{
			SetCollision(true, bBlockActors);
		}
	}
	// Turn OFF
	else if (Action.InputLinks[1].bHasImpulse)
	{
		if(bCollideActors)
		{
			SetCollision(false, bBlockActors);
		}
	}
	// Toggle
	else if (Action.InputLinks[2].bHasImpulse)
	{
		SetCollision(!bCollideActors, bBlockActors);
	}

	ForceNetRelevant();

	SetForcedInitialReplicatedProperty(Property'Engine.Actor.bCollideActors', (bCollideActors == default.bCollideActors));
}

simulated event CollisionChanged()
{
	// rigid body collision should match Unreal collision
	CollisionComponent.SetBlockRigidBody(bCollideActors && bBlockActors);
}

event ProcessActorSetVolume( Actor Other );

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=true
		bAcceptsLights=true
		LightingChannels=(Dynamic=TRUE,bInitialized=TRUE)
		BlockActors=false
		BlockZeroExtent=false
		BlockNonZeroExtent=true
		BlockRigidBody=false
		AlwaysLoadOnClient=True
		AlwaysLoadOnServer=True
		bDisableAllRigidBody=true
	End Object

	bCollideActors=True
	bSkipActorPropertyReplication=true
}
