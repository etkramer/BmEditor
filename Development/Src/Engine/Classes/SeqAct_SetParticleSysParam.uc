/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */


class SeqAct_SetParticleSysParam extends SequenceAction;

var() editinline array<ParticleSystemComponent.ParticleSysParam>	InstanceParameters;

/** Should ScalarValue override any entries to InstanceParameters? */
var() bool bOverrideScalar;

// BM
var() bool bOverrideVector;
// BM
var() bool bOverrideActor;

/** Override scalar value */
var() float ScalarValue;

// BM
var() vector VectorValue;
// BM
var() Actor ActorValue;

defaultproperties
{
	ObjName="Set Particle Param"
	ObjCategory="Particles"

	bOverrideScalar=TRUE

	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Scalar Value",PropertyName=ScalarValue)
}
