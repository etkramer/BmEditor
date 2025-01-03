/*=============================================================================
Landscape.cpp: New terrain rendering
Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"
#include "LandscapeDataAccess.h"
#include "LandscapeRender.h"

IMPLEMENT_CLASS(ULandscapeComponent)

//
// ULandscapeComponent
//
void ULandscapeComponent::AddReferencedObjects(TArray<UObject*>& ObjectArray)
{
	Super::AddReferencedObjects(ObjectArray);
	if(LightMap != NULL)
	{
		LightMap->AddReferencedObjects(ObjectArray);
	}
}

void ULandscapeComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.Ver() >= VER_LANDSCAPECOMPONENT_LIGHTMAPS)
	{
		Ar << LightMap;
	}

#if PS3
	if( Ar.IsLoading() || Ar.IsCountingMemory() )
	{
		Ar << PlatformDataSize;
		if( PlatformDataSize )
		{
			PlatformData = appMalloc(PlatformDataSize, 16);
			Ar.Serialize(PlatformData, PlatformDataSize);
		}
	}
#endif

#if WITH_EDITOR
	if( Ar.IsSaving() && (GCookingTarget & UE3::PLATFORM_PS3) )
	{
		Ar << PlatformDataSize;
		if( PlatformDataSize )
		{
			Ar.Serialize(PlatformData, PlatformDataSize);
		}
	}
#endif
}

#if WITH_EDITOR
/**
 * Generate a key for this component's layer allocations to use with MaterialInstanceConstantMap.
 */

IMPLEMENT_COMPARE_CONSTREF( FString, Landscape, { return A < B ? 1 : -1; } );

FString ULandscapeComponent::GetLayerAllocationKey() const
{
	FString Result;
	//ALandscape* Landscape = GetLandscapeActor();

	// Sort the allocations
	TArray<FString> LayerStrings;
	for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
	{
		new(LayerStrings) FString( *FString::Printf(TEXT("%s_%d_"), *WeightmapLayerAllocations(LayerIdx).LayerName.ToString(), WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex) );
	}
	Sort<USE_COMPARE_CONSTREF(FString,Landscape)>( &LayerStrings(0), LayerStrings.Num() );

	for( INT LayerIdx=0;LayerIdx < LayerStrings.Num();LayerIdx++ )
	{
		Result += LayerStrings(LayerIdx);
	}
	return Result;
}

void ULandscapeComponent::GetLayerDebugColorKey(INT& R, INT& G, INT& B) const
{
	ALandscape* Landscape = GetLandscapeActor();
	if (Landscape)
	{
		R = INDEX_NONE, G = INDEX_NONE, B = INDEX_NONE;
		for ( INT LIndex = 0; LIndex < Landscape->LayerInfos.Num(); LIndex++)
		{
			if (Landscape->LayerInfos(LIndex).DebugColorChannel > 0)
			{
				for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )	
				{
					if (WeightmapLayerAllocations(LayerIdx).LayerName == Landscape->LayerInfos(LIndex).LayerName)
					{
						if ( Landscape->LayerInfos(LIndex).DebugColorChannel & 1 ) // R
						{
							R = (WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex*4+WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel);
						}
						if ( Landscape->LayerInfos(LIndex).DebugColorChannel & 2 ) // G
						{
							G = (WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex*4+WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel);
						}
						if ( Landscape->LayerInfos(LIndex).DebugColorChannel & 4 ) // B
						{
							B = (WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex*4+WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel);
						}
						break;
					}
				}		
			}
		}
		//Result = *FString::Printf(TEXT("R.%d-G.%d-B.%d"), R, G, B);		
	}
}

void ALandscape::UpdateDebugColorMaterial()
{
	FlushRenderingCommands();
	//GWarn->BeginSlowTask( *FString::Printf(TEXT("Compiling layer color combinations for %s"), *GetName()), TRUE);
	TArray<ULandscapeComponent*> AllLandscapeComponents;
	GetAllEditableComponents(&AllLandscapeComponents);
	// Iterate all the components currently visible...
	for( INT Idx=0;Idx<AllLandscapeComponents.Num();Idx++ )
	{
		if (AllLandscapeComponents(Idx) && AllLandscapeComponents(Idx)->EditToolRenderData)
		{
			AllLandscapeComponents(Idx)->EditToolRenderData->UpdateDebugColorMaterial();
		}
	}
	FlushRenderingCommands();
	//GWarn->EndSlowTask();
}

void ULandscapeComponent::PostLoad()
{
	Super::PostLoad();
	SetupActor();
}

void ULandscapeComponent::PostRename()
{
	Super::PostRename();
	//SetupActor();
}

void ULandscapeComponent::PostEditImport()
{
	Super::PostEditImport();
	//SetupActor();
}
#endif // WITH_EDITOR

// Streaming Texture info
void ULandscapeComponent::GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	ALandscape* Landscape = GetLandscapeActor();
	ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(GetOuter());
	FSphere BoundingSphere = Bounds.GetSphere();
	float StreamingDistanceMultiplier = 1.f;
	if (Landscape)
	{
		StreamingDistanceMultiplier = Landscape->StreamingDistanceMultiplier;
	}
	const FLOAT TexelFactor = 0.75f * StreamingDistanceMultiplier * ComponentSizeQuads * Proxy->DrawScale * Proxy->DrawScale3D.X;

	// Enumerate the material's textures.
	TArray<UTexture*> Textures;
	if (MaterialInstance)
	{
		UMaterial* Material = MaterialInstance->GetMaterial();
		if (Material)
		{
/*
			MaterialInstance->GetMaterial()->GetUsedTextures(Textures);
			// Add each texture to the output with the appropriate parameters.
			for(INT TextureIndex = 0;TextureIndex < Textures.Num();TextureIndex++)
			{
				UTexture2D* Texture = Cast<UTexture2D>(Textures(TextureIndex));
				if (Texture)
				{
					FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
					StreamingTexture.Bounds = BoundingSphere;
					StreamingTexture.TexelFactor = TexelFactor;
					StreamingTexture.Texture = Textures(TextureIndex);
				}
			}
*/

			INT NumExpressions = Material->Expressions.Num();
			for(INT ExpressionIndex = 0;ExpressionIndex < NumExpressions; ExpressionIndex++)
			{
				UMaterialExpression* Expression = Material->Expressions(ExpressionIndex);
				UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression);
				UMaterialExpressionTextureSampleParameter2D* TextureSampleParameter = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression);

				if(TextureSample && TextureSample->Coordinates.Expression)
				{
					UMaterialExpressionTextureCoordinate* TextureCoordinate =
						Cast<UMaterialExpressionTextureCoordinate>( TextureSample->Coordinates.Expression );

					UMaterialExpressionTerrainLayerCoords* TerrainTextureCoordinate =
						Cast<UMaterialExpressionTerrainLayerCoords>( TextureSample->Coordinates.Expression );

					if ( TextureCoordinate )
					{
						FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
						StreamingTexture.Bounds = BoundingSphere;
						StreamingTexture.TexelFactor = TexelFactor * Max(TextureCoordinate->UTiling, TextureCoordinate->VTiling);
						StreamingTexture.Texture = TextureSample->Texture;
					}
					else if ( TerrainTextureCoordinate )
					{
						FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
						StreamingTexture.Bounds = BoundingSphere;
						StreamingTexture.TexelFactor = TexelFactor * TerrainTextureCoordinate->MappingScale;
						StreamingTexture.Texture = TextureSample->Texture;
						
					}
					else
					{
						// Ignore.
						continue;
					}
				}
			}
		}
	}

	// Weightmap
	for(INT TextureIndex = 0;TextureIndex < WeightmapTextures.Num();TextureIndex++)
	{
		FStreamingTexturePrimitiveInfo& StreamingWeightmap = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
		StreamingWeightmap.Bounds = BoundingSphere;
		StreamingWeightmap.TexelFactor = TexelFactor;
		StreamingWeightmap.Texture = WeightmapTextures(TextureIndex);
	}

	// Heightmap
	FStreamingTexturePrimitiveInfo& StreamingHeightmap = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
	StreamingHeightmap.Bounds = BoundingSphere;
	StreamingHeightmap.TexelFactor = TexelFactor;
	StreamingHeightmap.Texture = HeightmapTexture;
}

ALandscape* ULandscapeComponent::GetLandscapeActor() const
{
	ALandscapeProxy* Landscape = CastChecked<ALandscapeProxy>(GetOuter());
	if (Landscape)
	{
		return Landscape->GetLandscapeActor();
	}
	return NULL;
}

ALandscapeProxy* ULandscapeComponent::GetLandscapeProxy() const
{
	return CastChecked<ALandscapeProxy>(GetOuter());
}

ALandscape* ULandscapeHeightfieldCollisionComponent::GetLandscapeActor() const
{
	ALandscapeProxy* Landscape = CastChecked<ALandscapeProxy>(GetOuter());
	if (Landscape)
	{
		return Landscape->GetLandscapeActor();
	}
	return NULL;
}

ALandscapeProxy* ULandscapeHeightfieldCollisionComponent::GetLandscapeProxy() const
{
	return CastChecked<ALandscapeProxy>(GetOuter());
}

void ULandscapeComponent::BeginDestroy()
{
	Super::BeginDestroy();

#if PS3 || WITH_EDITOR
	if( PlatformData )
	{
		appFree(PlatformData);
		PlatformDataSize = 0;
	}
#endif

#if WITH_EDITOR
	if( GIsEditor && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		ALandscape* Landscape = GetLandscapeActor();
		ALandscapeProxy* Proxy = GetLandscapeProxy();
		
		if (Landscape)
		{
			Landscape->XYtoComponentMap.Remove(ALandscape::MakeKey(SectionBaseX,SectionBaseY));

			// Remove any weightmap allocations from the Landscape Actor's map
			for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
			{
				UTexture2D* WeightmapTexture = WeightmapTextures(WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
				FLandscapeWeightmapUsage* Usage = Proxy->WeightmapUsageMap.Find(WeightmapTexture);
				if( Usage != NULL )
				{
					Usage->ChannelUsage[WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel] = NULL;

					if( Usage->FreeChannelCount()==4 )
					{
						Proxy->WeightmapUsageMap.Remove(WeightmapTexture);
					}
				}
			}
		}
	}

	if( EditToolRenderData != NULL )
	{
		// Ask render thread to destroy EditToolRenderData
		EditToolRenderData->Cleanup();
		EditToolRenderData = NULL;
	}
#endif
}

FPrimitiveSceneProxy* ULandscapeComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
#if WITH_EDITOR
	if( EditToolRenderData == NULL )
	{
		EditToolRenderData = new FLandscapeEditToolRenderData(this);
	}
	Proxy = new FLandscapeComponentSceneProxy(this, EditToolRenderData);
#else
	Proxy = new FLandscapeComponentSceneProxy(this, NULL);
#endif
	return Proxy;
}

void ULandscapeComponent::UpdateBounds()
{
	Bounds = CachedBoxSphereBounds;
#if !CONSOLE
	if ( !GIsGame && Scene->GetWorld() == GWorld )
	{
		ULevel::TriggerStreamingDataRebuild();
	}
#endif
}

void ULandscapeComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(FTranslationMatrix(FVector(SectionBaseX,SectionBaseY,0)) * ParentToWorld);
}

/** 
* Retrieves the materials used in this component 
* 
* @param OutMaterials	The list of used materials.
*/
void ULandscapeComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	if( MaterialInstance != NULL )
	{
		OutMaterials.AddItem(MaterialInstance);
	}
}


//
// ALandscape
//
void ALandscapeProxy::InitRBPhys()
{
#if WITH_NOVODEX
	if (!GWorld->RBPhysScene)
	{
		return;
	}
#endif // WITH_NOVODEX

	for(INT ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++ )
	{
		ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents(ComponentIndex);
		if( Comp && Comp->IsAttached() )
		{
			Comp->InitComponentRBPhys(TRUE);
		}
	}
}

void ALandscapeProxy::UpdateComponentsInternal(UBOOL bCollisionUpdate)
{
	Super::UpdateComponentsInternal(bCollisionUpdate);

	const FMatrix&	ActorToWorld = LocalToWorld();

	// Render components
	for(INT ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++ )
	{
		ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
		if( Comp )
		{
			Comp->UpdateComponent(GWorld->Scene,this,ActorToWorld);
		}
	}

	// Collision components
	for(INT ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++ )
	{
		ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents(ComponentIndex);
		if( Comp )
		{
			Comp->UpdateComponent(GWorld->Scene,this,ActorToWorld);
		}
	}
}

#if WITH_EDITOR
void ALandscape::UpdateComponentsInternal(UBOOL bCollisionUpdate)
{
	Super::UpdateComponentsInternal(bCollisionUpdate);
}
#endif

// FLandscapeWeightmapUsage serializer
FArchive& operator<<( FArchive& Ar, FLandscapeWeightmapUsage& U )
{
	return Ar << U.ChannelUsage[0] << U.ChannelUsage[1] << U.ChannelUsage[2] << U.ChannelUsage[3];
}

FArchive& operator<<( FArchive& Ar, FLandscapeAddCollision& U )
{
	return Ar << U.Corners[0] << U.Corners[1] << U.Corners[2] << U.Corners[3];
}

void ALandscape::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// We do not serialize XYtoComponentMap as we don't want these references to hold a component in memory.
	// The references are automatically cleaned up by the components' BeginDestroy method.
	if (Ar.IsTransacting())
	{
		Ar << XYtoAddCollisionMap;
		Ar << XYtoComponentMap;
		Ar << XYtoCollisionComponentMap;
	}
}

void ALandscape::PostLoad()
{
	Super::PostLoad();

	// Temporary
	if( ComponentSizeQuads == 0 && LandscapeComponents.Num() > 0 )
	{
		ULandscapeComponent* Comp = LandscapeComponents(0);
		if( Comp )
		{
			ComponentSizeQuads = Comp->ComponentSizeQuads;
			SubsectionSizeQuads = Comp->SubsectionSizeQuads;	
			NumSubsections = Comp->NumSubsections;
		}
	}

	// For the LayerName legacy
	if (LayerNames_DEPRECATED.Num() && !LayerInfos.Num())
	{
		for (INT Idx = 0; Idx < LayerNames_DEPRECATED.Num(); Idx++)
		{
			new(LayerInfos) FLandscapeLayerInfo(LayerNames_DEPRECATED(Idx));
		}
	}

#if WITH_EDITOR
	if( GIsEditor )
	{
		// For data validation..
		for (INT Idx = 0; Idx < LayerInfos.Num(); Idx++)
		{
			LayerInfos(Idx).Hardness = Clamp<FLOAT>(LayerInfos(Idx).Hardness, 0.f, 1.f);
			LayerInfos(Idx).DebugColorChannel = 0;
		}

		if (GetLinker() && (GetLinker()->Ver() < VER_CHANGED_LANDSCAPE_MATERIAL_PARAMS))
		{
			GWarn->BeginSlowTask( TEXT("Updating Landscape material combinations"), TRUE);

			// Clear any RF_Standalone flag for material instance constants in the level package
			// So it can be GC'd when it's no longer used.
			UObject* Pkg = GetOutermost();
			for ( TObjectIterator<UMaterialInstanceConstant> It; It; ++It )
			{
				if( (*It)->GetOutermost() == Pkg )
				{
					(*It)->ClearFlags(RF_Standalone);

					// Clear out the parent for any old MIC's
					(*It)->SetParent(NULL);
				}
			}

			// Recreate MIC's for all components
			for(INT ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++ )
			{
				GWarn->StatusUpdatef( ComponentIndex, LandscapeComponents.Num(), TEXT("Updating Landscape material combinations") );
				ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
				if( Comp )
				{
					Comp->UpdateMaterialInstances();
				}
			}

			// Clear out the thumbnail MICs so they're recreated
			for (INT LayerIdx = 0; LayerIdx < LayerInfos.Num(); LayerIdx++)
			{
				LayerInfos(LayerIdx).ThumbnailMIC = NULL;
			}

			GWarn->EndSlowTask();
		}
	}
#endif
}

void ALandscape::ClearComponents()
{
	Super::ClearComponents();
#if WITH_EDITOR
	//for (TSet<ALandscapeProxy*>::TIterator It(Proxies); It; ++It)
	//{
	//	ALandscapeProxy* Proxy = *It;
	//	//Proxy->ClearComponents();
	//}
	//Proxies.Empty();
#endif
}

void ALandscape::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	if( DataInterface )
	{
		delete DataInterface;
		DataInterface = NULL;
	}
	//TSet<ALandscapeProxy*> TempProxies = Proxies;
	//for (TSet<ALandscapeProxy*>::TIterator It(TempProxies); It; ++It )
	//{
	//	ALandscapeProxy* Proxy = *It;
	//	if (Proxy)
	//	{
	//		Proxy->ConditionalDestroy();
	//	}
	//}
#endif
}

void ALandscape::ClearCrossLevelReferences()
{
	Super::ClearCrossLevelReferences();
	//for (TSet<ALandscapeProxy*>::TIterator It(Proxies); It; ++It )
	//{
	//	ALandscapeProxy* Proxy = *It;
	//	if (Proxy)
	//	{
	//		Proxy->ClearCrossLevelReferences();
	//	}
	//}
}

IMPLEMENT_CLASS(ALandscape);

void ALandscapeProxy::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if( !(Ar.IsLoading() || Ar.IsSaving()) )
	{
		Ar << MaterialInstanceConstantMap;
	}

	// For Undo
	if( Ar.IsTransacting() )
	{
		Ar << WeightmapUsageMap;
	}
}

void ALandscapeProxy::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	UpdateLandscapeActor(NULL);

	GEngine->DeferredCommands.AddUniqueItem(TEXT("UpdateAddLandscapeComponents"));
#endif
}

void ALandscapeProxy::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	if (bIsProxy && LandscapeActor)
	{
		for (TSet<ALandscapeProxy*>::TIterator It(LandscapeActor->Proxies); It; ++It )
		{
			if ((*It) == this)
			{
				It.RemoveCurrent();
				break;
			}
		}
	}
#endif
}

void ALandscapeProxy::ClearComponents()
{
	// wait until resources are released
	FlushRenderingCommands();

	Super::ClearComponents();

	// Render components
	for(INT ComponentIndex = 0;ComponentIndex < LandscapeComponents.Num();ComponentIndex++)
	{
		ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
		if (Comp)
		{
			Comp->ConditionalDetach();
		}
	}

	// Collision components
	for(INT ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++ )
	{
		ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents(ComponentIndex);
		if( Comp )
		{
			Comp->ConditionalDetach();
		}
	}
}

ALandscape* ALandscape::GetLandscapeActor()
{
	return this;
}

ALandscape* ALandscapeProxy::GetLandscapeActor()
{
	return LandscapeActor;
}

void ALandscapeProxy::ClearCrossLevelReferences()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// delay the reset of the material until after the save has finished
		GEngine->DeferredCommands.AddUniqueItem(TEXT("RestoreLandscapeAfterSave"));
	}
}

void ALandscapeProxy::RestoreLandscapeAfterSave()
{
	for (FActorIterator It; It; ++It)
	{
		ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
		if (Proxy && Proxy->bIsProxy)
		{
			//Proxy->ConditionalUpdateComponents();
			Proxy->MarkComponentsAsDirty();
		}
	}
}

void ALandscapeProxy::GetSharedProperties(ALandscape* Landscape)
{
	if (Landscape)
	{
		Location = Landscape->Location;
		PrePivot = Landscape->PrePivot;
		DrawScale = Landscape->DrawScale;
		DrawScale3D = Landscape->DrawScale3D;
		StaticLightingResolution = Landscape->StaticLightingResolution;
	}
}

#if WITH_EDITOR
void ALandscapeProxy::PreSave()
{
	Super::PreSave();
	if (bIsProxy && LandscapeActor)
	{
		LandscapeActorName = LandscapeActor->GetPathName();
		GetSharedProperties(LandscapeActor);
	}
}

void ALandscape::PreSave()
{
	Super::PreSave();
	for (TSet<ALandscapeProxy*>::TIterator It(Proxies); It; ++It)
	{
		ALandscapeProxy* Proxy = *It;
		Proxy->LandscapeActorName = GetPathName();
		Proxy->LandscapeActor = this;
		Proxy->GetSharedProperties(this);
	}
}
#endif

IMPLEMENT_CLASS(ALandscapeProxy);
