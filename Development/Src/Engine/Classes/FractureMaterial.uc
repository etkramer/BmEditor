/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class FractureMaterial extends Object
	native(Physics)
	collapsecategories
	hidecategories(Object);

/** Particle system effect to play at fracture location. */
var()	ParticleSystem				FractureEffect;
// BM: AkEvent
var()	Object						FractureShardSound;
// BM: AkEvent
var()	Object						FractureDamageSound;

// BM
var()	bool	CastShadow;
var()	const bool	bForceDirectLightMap;
var()	bool	bCastDynamicShadow;
var()	bool	bSelfShadowOnly;
var()	bool	bAcceptsDynamicDominantLightShadows;
var()	bool	bCastHiddenShadow;
var()	bool	bCastShadowAsTwoSided;
var()	const bool	bAcceptsLights;
var()	const bool	bAcceptsDynamicLights;
var()	const bool	bUseOnePassLightingOnTranslucency;
var()	const bool	bUsePrecomputedShadows;
var()	const bool	bCastStaticModulatedShadows;
var()	const bool	bRecieveStaticModulatedShadows;
var()	bool	bCullModulatedShadowOnEmissive;
var()	bool	bAllowAmbientOcclusion;
var()	const LightingChannelContainer	LightingChannels;

defaultproperties
{
}
