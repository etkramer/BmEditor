/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __DYNAMICLIGHTENVIRONMENTCOMPONENT_H__
#define __DYNAMICLIGHTENVIRONMENTCOMPONENT_H__

#include "PrecomputedLightVolume.h"

#if BATMAN
/**
 * Global DLEC operating mode. In retail BM2 this is a process-wide enum that gates whether
 * the secondary lighting produced by FDynamicLightEnvironmentState is emitted as the legacy
 * SH/Sky pair or as a single UAmbientPlus3DirectionalLightComponent.
 */
enum EDLEC_Mode
{
	DLEC_Unmolested = 0,
	DLEC_APlus3D    = 1,
};
extern EDLEC_Mode GDLEC_Mode;

class UDynamicLightEnvironmentComponent;

namespace LightEnvironmentTick
{
	/** Queues a full update of Component's environment, to be performed by the next TickDeferredResets. */
	void QueueDeferredReset(UDynamicLightEnvironmentComponent* Component);

	/** Drops a queued update, so a destroyed component isn't left in the queue. */
	void CancelDeferredReset(UDynamicLightEnvironmentComponent* Component);

	/** Drains the deferred reset queue, fully updating each queued environment. */
	void TickDeferredResets();
}

struct FDirectionalApproximation
{
	FLinearColor Colour;
	FVector Direction;
	FLOAT Intensity;

	FDirectionalApproximation()
	:	Colour(FLinearColor::Black)
	,	Direction(0,0,1)
	,	Intensity(0.0f)
	{}

	FDirectionalApproximation(const FLinearColor& InColour,const FVector& InDirection)
	:	Colour(InColour)
	,	Direction(InDirection)
	,	Intensity(appSqrt(Square(InColour.R) + Square(InColour.G) + Square(InColour.B)))
	{}

	FDirectionalApproximation(FLOAT InIntensity,const FVector& InDirection)
	:	Colour(FLinearColor(0,0,0,1))
	,	Direction(InDirection)
	,	Intensity(InIntensity)
	{}
};

struct F3DPlusAState
{
	FVector Direction[3];
	FVector Colour[3];
	FVector Ambient;

	F3DPlusAState()
	{
		for (INT Index = 0; Index < 3; Index++)
		{
			Direction[Index] = FVector(0,0,1);
			Colour[Index] = FVector::ZeroVector;
		}
		Ambient = FVector::ZeroVector;
	}
};
#endif

/**
 * The character lighting stats.
 */
enum EDLEStats
{
	STAT_DynamicLightVisibilityTime = STAT_DLEFirstStat,
	STAT_StaticLightVisibilityTime,
	STAT_UpdateOwnerTime,
	STAT_UpdateStaticEnvironmentTime,
	STAT_InterpolateIndirectTime,
	STAT_UpdateDynamicEnvironmentTime,
	STAT_UpdateEnvironmentInterpolationTime,
	STAT_CreateLightsTime,
	STAT_NumEnvironments,
	STAT_StaticEnvironmentUpdates,
	STAT_DynamicEnvironmentUpdates,
	STAT_NumLitParticleComponents,
	STAT_NumParticleDLEs,
	STAT_DynamicLightEnvironmentComponentTickTime,
	STAT_ParticleDLETickTime
};

class FLightEnvironmentDebugInfo
{
public:
	UBOOL bShowBounds;
	UBOOL bShowVisibility;
	UBOOL bShowVolumeInterpolation;
	UBOOL bShowDominantLightTransition;
	UBOOL bShowStaticUpdates;
	UBOOL bShowCreateLights;
	UBOOL bShowPrimaryLight;
	UBOOL bShowSecondaryLight;
	UBOOL bShowDirectLightingOnly;
	UBOOL bShowIndirectLightingOnly;
	UBOOL bShowIndirectLightingShadowDirection;
	UBOOL bShowShadows;
	const UDynamicLightEnvironmentComponent* Component;

	FLightEnvironmentDebugInfo() :
		bShowBounds(FALSE),
		bShowVisibility(FALSE),
		bShowVolumeInterpolation(FALSE),
		bShowDominantLightTransition(FALSE),
		bShowStaticUpdates(FALSE),
		bShowCreateLights(FALSE),
		bShowPrimaryLight(TRUE),
		bShowSecondaryLight(TRUE),
		bShowDirectLightingOnly(FALSE),
		bShowIndirectLightingOnly(FALSE),
		bShowIndirectLightingShadowDirection(TRUE),
		bShowShadows(TRUE),
		Component(NULL)
	{}
};

class FLightEnvShadowInfo
{
public:

	FLightEnvShadowInfo() :
		ShadowDirection(0,0,-1),
		DominantShadowFactor(0.0f),
		DominantShadowIntensity(0,0,0),
		TotalShadowIntensity(0,0,0)
	{}

	FVector ShadowDirection;
	FLOAT DominantShadowFactor;
	FLinearColor DominantShadowIntensity;
	FLinearColor TotalShadowIntensity;
};

/** The private light environment state. */
class FDynamicLightEnvironmentState
{
public:

	/** Initialization constructor. */
	FDynamicLightEnvironmentState(UDynamicLightEnvironmentComponent* InComponent);

	/** Computes the bounds and various lighting relevance attributes of the owner and its primitives. */
	UBOOL UpdateOwner();

	/** Updates the contribution of static lights to the light environment. */
	void UpdateStaticEnvironment(ULightComponent* NewAffectingDominantLight);
	
	/** Updates the contribution of dynamic lights to the light environment. */
	void UpdateDynamicEnvironment();

	/** Interpolates toward the target light environment state. */
	void UpdateEnvironmentInterpolation(FLOAT DeltaTime,FLOAT TimeBetweenUpdates);

	/** Performs a full update of the light environment. */
	void Update();

	/** Updates the light environment's state. */
	void Tick(FLOAT DeltaTime);

	/** Creates a light to represent the light environment's composite lighting. */
	ULightComponent* CreateRepresentativeLight(const FVector& Direction,const FLinearColor& Intensity);

	/** Creates a light to represent the light environment's composite shadowing. */
	UPointLightComponent* CreateRepresentativeShadowLight();

	/** Detaches the light environment's representative lights. */
	void DetachRepresentativeLights(UBOOL bAllLights);

	/** Creates the lights to represent the character's light environment. */
	void CreateEnvironmentLightList(ULightComponent* NewAffectingDominantLight, FLOAT NewDominantShadowTransitionDistance, UBOOL bForceUpdate = FALSE);

	/** Builds a list of objects referenced by the state. */
	void AddReferencedObjects(TArray<UObject*>& ObjectArray);

	/** Forces a full update on the next Tick. */
	void ResetEnvironment();

	/** Cleans up preview components. */
	void ClearPreviewComponents();

	FLOAT GetDominantShadowTransitionDistance() const { return CurrentDominantShadowTransitionDistance; }

	const FBoxSphereBounds& GetOwnerBounds()
	{
		return OwnerBounds;
	}

	/** Adds lights that affect this DLE to RelevantLightList. */
	void AddRelevantLights(TArray<ALight*>& RelevantLightList, UBOOL bDominantOnly) const;

private:

	/** The component which this is state for. */
	UDynamicLightEnvironmentComponent* Component;

	/** The bounds of the owner. */
	FBoxSphereBounds OwnerBounds;

	/** The predicted center of the owner at the time of the next update. */
	FVector PredictedOwnerPosition;

	/** The lighting channels the owner's primitives are affected by. */
	FLightingChannelContainer OwnerLightingChannels;

	/** The owner's level. */
	UPackage* OwnerPackage;

	/** Used for planar shadows on Mobile platforms. */
	FPlane ShadowPlane;

	/** The time the light environment was last updated. */
	FLOAT LastUpdateTime;

	/** Time between updates for invisible objects. */
	FLOAT InvisibleUpdateTime;

	/** Min time between full environment updates. */
	FLOAT MinTimeBetweenFullUpdates;

	FVector LastInterpolatePosition;

	/** Active distance from the edge of OwnerBounds to the nearest dominant shadow transition. */
	FLOAT CurrentDominantShadowTransitionDistance;

	/** A pool of unused light components. */
	mutable TArray<ULightComponent*> RepresentativeLightPool;

	/** The character's current static light environment. */
	FSHVectorRGB StaticLightEnvironment;
	/** The current static non-shadowed light environment. */
	FSHVectorRGB StaticNonShadowedLightEnvironment;
	/** The current static shadow environment. */
	FLightEnvShadowInfo StaticShadowInfo;

	/** The current dynamic light environment. */
	FSHVectorRGB DynamicLightEnvironment;
	/** The current dynamic non-shadowed light environment. */
	FSHVectorRGB DynamicNonShadowedLightEnvironment;
	/** The current dynamic shadow environment. */
	FLightEnvShadowInfo DynamicShadowInfo;

	/** New static light environment to interpolate to. */
	FSHVectorRGB NewStaticLightEnvironment;
	/** New static non-shadowed light environment to interpolate to. */
	FSHVectorRGB NewStaticNonShadowedLightEnvironment;
	/** New static shadow environment to interpolate to. */
	FLightEnvShadowInfo NewStaticShadowInfo;

	/** The lighting which was used to create the current representative lights. */
	FSHVectorRGB CurrentRepresentativeLightEnvironment;
	/** The non-shadowed lighting which was used to create the current representative lights. */
	FSHVectorRGB CurrentRepresentativeNonShadowedLightEnvironment;
	/** The lighting which was used to create the current representative shadow. */
	FLightEnvShadowInfo CurrentShadowInfo;

	/** The current representative shadow-casting light. */
	UPointLightComponent* CurrentRepresentativeShadowLight;

	/** Whether light environment has been fully updated at least once. */
	BITFIELD bFirstFullUpdate : 1;

	/** The positions relative to the owner's bounding box which are sampled for light visibility. */
	TArray<FVector> LightVisibilitySamplePoints;

#if BATMAN
	/** Whether the gathered lighting changed, so the AP3D interpolation needs a new target. */
	BITFIELD bInterpolationTargetUpdateRequired : 1;

	TArray<FDirectionalApproximation> StaticDirectionalApproximations;
	TArray<FDirectionalApproximation> DynamicDirectionalApproximations;
	FLinearColor StaticAmbientColour;
	FLinearColor DynamicAmbientColour;
	FLOAT AccumulatedAlpha;
	F3DPlusAState Previous3DPlusAState;
	FLightEnvShadowInfo Previous3DPlusAShadowInfo;
	F3DPlusAState Current3DPlusAState;
	FLightEnvShadowInfo Current3DPlusAShadowInfo;
	F3DPlusAState Next3DPlusAState;
	FLightEnvShadowInfo Next3DPlusAShadowInfo;
#endif

	//@todo - remove these in a shipping build
	TArray<FVolumeLightingSample> DebugInterpolatedVolumeSamples;
	TArray<ULightComponent*> DebugVolumeSampleLights;
	TArray<UStaticMeshComponent*> DebugVolumeSampleMeshes;
	mutable TArray<FDebugShadowRay> DebugStaticVisibilityTraces;
	mutable TArray<FDebugShadowRay> DebugDynamicVisibilityTraces;
	TArray<FDebugShadowRay> DebugClosestDominantLightRays;
	TArray<FVector> DebugStaticUpdates;
	TArray<FVector> DebugCreateLights;

	/**
	 * Determines whether a light is visible, using cached results if available.
	 * @param Light - The light to test visibility for.
	 * @param OwnerPosition - The position of the owner to compute the light's effect for.
	 * @param OutVisibilityFactor - Upon return, contains an appromiate percentage of light that reaches the owner's primitives.
	 * @return TRUE if the light reaches the owner's primitives.
	 */
	UBOOL IsLightVisible(const ULightComponent* Light, const FVector& OwnerPosition, UBOOL bIsDynamic, FLOAT& OutVisibilityFactor) const;

	/** Allocates a light, attempting to reuse a light with matching type from the free light pool. */
	template<typename LightType>
	LightType* AllocateLight() const;

	/**
	 * Tests whether a light affects the owner.
	 * @param Light - The light to test.
	 * @param OwnerPosition - The position of the owner to compute the light's effect for.
	 * @return TRUE if the light affects the owner.
	 */
	UBOOL DoesLightAffectOwner(const ULightComponent* Light, const FVector& OwnerPosition) const;

	/**
	 * Adds the light's contribution to the light environment.
	 * @param	Light						light to add
	 * @param	LightEnvironment			light environment to add the light's contribution to
	 * @param	NonShadowedLightEnvironment	light environment to add the non-shadowed part of the light's contribution to
	 * @param	ShadowEnvironment			The shadow environment to add the light's shadowing to.
	 * @param	OwnerPosition				The position of the owner to compute the light's effect for.
	 * @param	bIsDynamic					Whether the light is dynamic.
	 */
	void AddLightToEnvironment(
		const ULightComponent* Light, 
		FSHVectorRGB& LightEnvironment,
		FSHVectorRGB& NonShadowedLightEnvironment,
		FSHVectorRGB& ShadowEnvironment,
		const FBoxSphereBounds& OwnerBounds,
		UBOOL bIsDynamic
		);

#if BATMAN
	void AddAPlus3DAmbient(const FLinearColor& Colour,UBOOL bIsDynamic);
	void RebuildAPlus3DState();
	void BuildAPlus3DShadowInfo(const TArray<FDirectionalApproximation>& Approximations,const FLinearColor& AmbientColour);
	void InterpolateAPlus3DState(FLOAT Alpha);
#endif

	/** 
	 * Calculates the minimum distance to a dominant shadow transition, or 0 if not shadowed by a dominant light.
	 * NewAffectingDominantLight will be the dominant light whose shadow transition is closest.
	 */
	void CalculateDominantShadowTransitionDistance(ULightComponent*& NewAffectingDominantLight, FLOAT& NewDominantShadowTransitionDistance);

	friend void DrawLightEnvironmentDebugInfo(const FSceneView* View, FPrimitiveDrawInterface* PDI);
};

#endif
