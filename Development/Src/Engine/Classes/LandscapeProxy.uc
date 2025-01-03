/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class LandscapeProxy extends Info
	native(Terrain)
	hidecategories(Display, Attachment, Physics, Debug, Lighting);

/** The array of LandscapeComponent that are used by the landscape */
var const array<LandscapeComponent>	LandscapeComponents;

/** Array of LandscapeHeightfieldCollisionComponent */
var const array<LandscapeHeightfieldCollisionComponent>	CollisionComponents;

/** Structure storing channel usage for weightmap textures */
struct native LandscapeWeightmapUsage
{
	var LandscapeComponent ChannelUsage[4];

	structcpptext
	{
		// tor
		FLandscapeWeightmapUsage()
		{
			ChannelUsage[0] = NULL;
			ChannelUsage[1] = NULL;
			ChannelUsage[2] = NULL;
			ChannelUsage[3] = NULL;
		}

		// Serializer
		friend FArchive& operator<<( FArchive& Ar, FLandscapeWeightmapUsage& U );

		INT FreeChannelCount() const
		{
			return	((ChannelUsage[0] == NULL) ? 1 : 0) + 
					((ChannelUsage[1] == NULL) ? 1 : 0) + 
					((ChannelUsage[2] == NULL) ? 1 : 0) + 
					((ChannelUsage[3] == NULL) ? 1 : 0);
		}
	}
};

/** Map of material instance constants used to for the components. Key is generated with ULandscapeComponent::GetLayerAllocationKey() */
var const native map{FString ,class UMaterialInstanceConstant*} MaterialInstanceConstantMap;

/** Map of weightmap usage */
var const native map{UTexture2D*,struct FLandscapeWeightmapUsage} WeightmapUsageMap;

/**
 *	The resolution to cache lighting at, in texels/patch.
 *	A separate shadow-map is used for each terrain component, which is up to
 *	(MaxComponentSize * StaticLightingResolution + 1) pixels on a side.
 *	Must be a power of two, 1 <= StaticLightingResolution <= MaxTesselationLevel.
 */
var(Lighting) int				StaticLightingResolution;

var(LandscapeProxy) editoronly string LandscapeActorName;
var editoronly transient Landscape LandscapeActor;
// 
var const bool bIsProxy;

cpptext
{
	// AActor interface
	virtual UBOOL ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags);
	virtual void UpdateComponentsInternal(UBOOL bCollisionUpdate = FALSE);
	virtual void ClearComponents();
	virtual void InitRBPhys();

	virtual class ALandscape* GetLandscapeActor();

	// Cross level things...
	virtual void ClearCrossLevelReferences();
	static void RestoreLandscapeAfterSave();

	void GetSharedProperties(class ALandscape* Landscape);
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PreSave();
	virtual void InitRBPhysEditor();

	virtual void PostEditMove(UBOOL bFinished);

	// Called before editor copy, TRUE allow export
	virtual UBOOL ShouldExport();
	// Called before editor paste, TRUE allow import
	virtual UBOOL ShouldImport(FString* ActorPropString);

	void RemoveInvalidWeightmaps();
	void UpdateLandscapeActor(class ALandscape* Landscape);
	UBOOL IsValidLandscapeActor(class ALandscape* Landscape);
#endif

	// UObject interface
	virtual void Serialize(FArchive& Ar);
	virtual void BeginDestroy();
	virtual void PostLoad();
}

defaultproperties
{
	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.S_Terrain'
	End Object

	DrawScale3D=(X=128.0,Y=128.0,Z=256.0)
	StaticLightingResolution=1
	bEdShouldSnap=True
	bCollideActors=True
	bBlockActors=True
	bWorldGeometry=True
	bStatic=True
	bNoDelete=True
	bHidden=False
	bMovable=False
	bLockLocation=True
	bIsProxy=True
}
 