/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class AnimNotify_ClothingMaxDistanceScale extends AnimNotify
	native(Anim);

/** The Particle system to play **/
var() float	StartScale;
var() float EndScale;


var() EMaxDistanceScaleMode	ScaleMode;

var   float	Duration;

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );
	virtual void NotifyEnd( class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime );
}

defaultproperties
{
	
	StartScale = 1;
	EndScale = 1;
	ScaleMode = MDSM_Multiply;
}

