/*=============================================================================
LandscapeEdit.cpp: Landscape editing
Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "UnTerrain.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeRender.h"

#if WITH_EDITOR

//
// ULandscapeComponent
//
void ULandscapeComponent::Init(INT InBaseX,INT InBaseY,INT InComponentSizeQuads, INT InNumSubsections,INT InSubsectionSizeQuads)
{
	SectionBaseX = InBaseX;
	SectionBaseY = InBaseY;
	ComponentSizeQuads = InComponentSizeQuads;
	NumSubsections = InNumSubsections;
	SubsectionSizeQuads = InSubsectionSizeQuads;
	check(NumSubsections * SubsectionSizeQuads == ComponentSizeQuads);
	GetLandscapeActor()->XYtoComponentMap.Set(ALandscape::MakeKey(SectionBaseX,SectionBaseY), this);
}

void ULandscapeComponent::UpdateCachedBounds()
{
	FLandscapeComponentDataInterface CDI(this);

	FBox Box(0);
	for( INT y=0;y<ComponentSizeQuads+1;y++ )
	{
		for( INT x=0;x<ComponentSizeQuads+1;x++ )
		{
			Box += CDI.GetWorldVertex(x,y);
		}
	}
	CachedBoxSphereBounds = FBoxSphereBounds(Box);

	// Update collision component bounds
	ALandscape* LandscapeActor = GetLandscapeActor();
	if (LandscapeActor)
	{
		QWORD ComponentKey = ALandscape::MakeKey(SectionBaseX,SectionBaseY);
		ULandscapeHeightfieldCollisionComponent* CollisionComponent = LandscapeActor->XYtoCollisionComponentMap.FindRef(ComponentKey);
		if( CollisionComponent )
		{
			CollisionComponent->Modify();
			CollisionComponent->CachedBoxSphereBounds = CachedBoxSphereBounds;	
			CollisionComponent->ConditionalUpdateTransform();
		}
	}
}

void ULandscapeComponent::UpdateMaterialInstances()
{
	check(GIsEditor);

	ALandscape* Landscape = GetLandscapeActor();
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	
	if( Landscape && Landscape->LandscapeMaterial != NULL )
	{
		FString LayerKey = GetLayerAllocationKey();
		// debugf(TEXT("Looking for key %s"), *LayerKey);

		// Find or set a matching MIC in the Landscape's map.
		UMaterialInstanceConstant* CombinationMaterialInstance = Proxy->MaterialInstanceConstantMap.FindRef(*LayerKey);
		if( CombinationMaterialInstance == NULL || CombinationMaterialInstance->Parent != Landscape->LandscapeMaterial || GetOutermost() != CombinationMaterialInstance->GetOutermost() )
		{
			CombinationMaterialInstance = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass(), GetOutermost(), FName(*FString::Printf(TEXT("%s_%s"),*Landscape->GetName(),*LayerKey)), RF_Public);
			debugf(TEXT("Looking for key %s, making new combination %s"), *LayerKey, *CombinationMaterialInstance->GetName());
			Proxy->MaterialInstanceConstantMap.Set(*LayerKey,CombinationMaterialInstance);
			UBOOL bNeedsRecompile;
			CombinationMaterialInstance->SetParent(Landscape->LandscapeMaterial);

			UMaterial* ParentUMaterial = CombinationMaterialInstance->GetMaterial();

			if( ParentUMaterial && ParentUMaterial != GEngine->DefaultMaterial )
			{
				ParentUMaterial->SetMaterialUsage(bNeedsRecompile,MATUSAGE_Landscape);
				ParentUMaterial->SetMaterialUsage(bNeedsRecompile,MATUSAGE_StaticLighting);
			}

			FStaticParameterSet StaticParameters;
			CombinationMaterialInstance->GetStaticParameterValues(&StaticParameters);

			// Find weightmap mapping for each layer parameter, or disable if the layer is not used in this component.
			for( INT LayerParameterIdx=0;LayerParameterIdx<StaticParameters.TerrainLayerWeightParameters.Num();LayerParameterIdx++ )
			{
				FStaticTerrainLayerWeightParameter& LayerParameter = StaticParameters.TerrainLayerWeightParameters(LayerParameterIdx);
				LayerParameter.WeightmapIndex = INDEX_NONE;

				// Look through our allocations to see if we need this layer.
				// If not found, this component doesn't use the layer, and WeightmapIndex remains as INDEX_NONE.
				for( INT AllocIdx=0;AllocIdx<WeightmapLayerAllocations.Num();AllocIdx++ )
				{
					FWeightmapLayerAllocationInfo& Allocation = WeightmapLayerAllocations(AllocIdx);
					if( Allocation.LayerName == LayerParameter.ParameterName )
					{
						LayerParameter.WeightmapIndex = Allocation.WeightmapTextureIndex;
						LayerParameter.bOverride = TRUE;
						// debugf(TEXT(" Layer %s channel %d"), *LayerParameter.ParameterName.ToString(), LayerParameter.WeightmapIndex);
						break;
					}
				}
			}

			if (CombinationMaterialInstance->SetStaticParameterValues(&StaticParameters))
			{
				//mark the package dirty if a compile was needed
				CombinationMaterialInstance->MarkPackageDirty();
			}

			CombinationMaterialInstance->InitResources();
			CombinationMaterialInstance->UpdateStaticPermutation();
		}
		else
		{
			// debugf(TEXT("Looking for key %s, found combination %s"), *LayerKey, *CombinationMaterialInstance->GetName());
		}

		// Create the instance for this component, that will use the layer combination instance.
		if( MaterialInstance == NULL || GetOutermost() != MaterialInstance->GetOutermost() )
		{
			MaterialInstance = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass(), GetOutermost(), FName(*FString::Printf(TEXT("%s_Inst"),*GetName())), RF_Public);
		}

		// For undo
		MaterialInstance->SetFlags(RF_Transactional);
		MaterialInstance->Modify();

		MaterialInstance->SetParent(CombinationMaterialInstance);

		FLinearColor Masks[4];
		Masks[0] = FLinearColor(1.f,0.f,0.f,0.f);
		Masks[1] = FLinearColor(0.f,1.f,0.f,0.f);
		Masks[2] = FLinearColor(0.f,0.f,1.f,0.f);
		Masks[3] = FLinearColor(0.f,0.f,0.f,1.f);

		// Set the layer mask
		for( INT AllocIdx=0;AllocIdx<WeightmapLayerAllocations.Num();AllocIdx++ )
		{
			FWeightmapLayerAllocationInfo& Allocation = WeightmapLayerAllocations(AllocIdx);
			MaterialInstance->SetVectorParameterValue(FName(*FString::Printf(TEXT("LayerMask_%s"),*Allocation.LayerName.ToString())), Masks[Allocation.WeightmapTextureChannel]);
		}

		// Set the weightmaps
		for( INT i=0;i<WeightmapTextures.Num();i++ )
		{
			// debugf(TEXT("Setting Weightmap%d = %s"), i, *WeightmapTextures(i)->GetName());
			MaterialInstance->SetTextureParameterValue(FName(*FString::Printf(TEXT("Weightmap%d"),i)), WeightmapTextures(i));
		}
		// Set the heightmap, if needed.
		MaterialInstance->SetTextureParameterValue(FName(TEXT("Heightmap")), HeightmapTexture);
	}
}

/** Called after an Undo action occurs */
void ULandscapeComponent::PostEditUndo()
{
	Super::PostEditUndo();
	UpdateMaterialInstances();
	if (EditToolRenderData)
	{
		EditToolRenderData->UpdateDebugColorMaterial();
	}
}

void ULandscapeComponent::SetupActor()
{
	if( GIsEditor && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		ALandscape* Landscape = GetLandscapeActor();
		ALandscapeProxy* Proxy = GetLandscapeProxy();

		if (Landscape)
		{
			// Store the components in the map
			Landscape->XYtoComponentMap.Set(ALandscape::MakeKey(SectionBaseX,SectionBaseY), this);

			TMap<FName, FLandscapeLayerInfo> LayerInfosMap;
			for (INT Idx = 0; Idx < Landscape->LayerInfos.Num(); Idx++)
			{
				LayerInfosMap.Set(Landscape->LayerInfos(Idx).LayerName, Landscape->LayerInfos(Idx));
			}
			TArray<FName> DeletedLayers;
			UBOOL bFixedLayerDeletion = FALSE;

			// LayerName Validation check...
			for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
			{
				FLandscapeLayerInfo* LayerInfo = LayerInfosMap.Find(WeightmapLayerAllocations(LayerIdx).LayerName);

				if (LayerInfo == NULL)
				{
					if(!bFixedLayerDeletion )
					{
						GWarn->MapCheck_Add( MCTYPE_WARNING, Proxy, *FString::Printf(TEXT("Fixed up deleted layer weightmap for %s."), *GetName() ), MCACTION_NONE );
						bFixedLayerDeletion = TRUE;
					}
					DeletedLayers.AddItem(WeightmapLayerAllocations(LayerIdx).LayerName);
				}
			}

			if (bFixedLayerDeletion)
			{
				FLandscapeEditDataInterface LandscapeEdit(Landscape);
				for (INT Idx = 0; Idx < DeletedLayers.Num(); ++Idx)
				{
					DeleteLayer(DeletedLayers(Idx), LayerInfosMap, &LandscapeEdit);
				}
			}

			UBOOL bFixedWeightmapTextureIndex=FALSE;

			// Store the weightmap allocations in WeightmapUsageMap
			for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
			{
				FWeightmapLayerAllocationInfo& Allocation = WeightmapLayerAllocations(LayerIdx);

				// Fix up any problems caused by the layer deletion bug.
				if( Allocation.WeightmapTextureIndex >= WeightmapTextures.Num() )
				{
					Allocation.WeightmapTextureIndex = WeightmapTextures.Num()-1; 
					if( !bFixedWeightmapTextureIndex )
					{
						GWarn->MapCheck_Add( MCTYPE_WARNING, Proxy, *FString::Printf(TEXT("Fixed up incorrect layer weightmap texture index for %s."), *GetName() ), MCACTION_NONE );
						bFixedWeightmapTextureIndex = TRUE;
					}
				}

				UTexture2D* WeightmapTexture = WeightmapTextures(Allocation.WeightmapTextureIndex);
				FLandscapeWeightmapUsage* Usage = Proxy->WeightmapUsageMap.Find(WeightmapTexture);
				if( Usage == NULL )
				{
					Usage = &Proxy->WeightmapUsageMap.Set(WeightmapTexture,FLandscapeWeightmapUsage());
				}

				// Detect a shared layer allocation, caused by a previous undo or layer deletion bugs
				if( Usage->ChannelUsage[Allocation.WeightmapTextureChannel] != NULL )
				{
					GWarn->MapCheck_Add( MCTYPE_WARNING, Proxy, *FString::Printf(TEXT("Fixed up shared weightmap texture for layer %s in component %s (shares with %s)."), *Allocation.LayerName.ToString(), *GetName(), *Usage->ChannelUsage[Allocation.WeightmapTextureChannel]->GetName() ), MCACTION_NONE );
					WeightmapLayerAllocations.Remove(LayerIdx);
					LayerIdx--;
					continue;
				}
				else
				{
					Usage->ChannelUsage[Allocation.WeightmapTextureChannel] = this;
				}
			}

			RemoveInvalidWeightmaps();

			// Store the layer combination in the MaterialInstanceConstantMap
			if( MaterialInstance != NULL )
			{
				UMaterialInstanceConstant* CombinationMaterialInstance = Cast<UMaterialInstanceConstant>(MaterialInstance->Parent);
				if( CombinationMaterialInstance )
				{
					Landscape->MaterialInstanceConstantMap.Set(*GetLayerAllocationKey(),CombinationMaterialInstance);
				}
			}
		}
	}
}

//
// LandscapeComponentAlphaInfo
//
struct FLandscapeComponentAlphaInfo
{
	INT LayerIndex;
	TArrayNoInit<BYTE> AlphaValues;

	// tor
	FLandscapeComponentAlphaInfo( ULandscapeComponent* InOwner, INT InLayerIndex )
	:	LayerIndex(InLayerIndex)
	,	AlphaValues(E_ForceInit)
	{
		AlphaValues.Empty(Square(InOwner->ComponentSizeQuads+1));
		AlphaValues.AddZeroed(Square(InOwner->ComponentSizeQuads+1));
	}

	UBOOL IsLayerAllZero() const
	{
		for( INT Index=0;Index<AlphaValues.Num();Index++ )
		{
			if( AlphaValues(Index) != 0 )
			{
				return FALSE;
			}
		}
		return TRUE;
	}
};

/**
 * Creates a collision component
 * @param HeightmapTextureMipData: heightmap data
 * @param ComponentX1, ComponentY1, ComponentX2, ComponentY2: region to update
 */
void ULandscapeComponent::UpdateCollisionComponent(FColor* HeightmapTextureMipData, INT ComponentX1, INT ComponentY1, INT ComponentX2, INT ComponentY2, UBOOL bUpdateBounds )
{
	ALandscape* LandscapeActor = GetLandscapeActor();
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	QWORD ComponentKey = ALandscape::MakeKey(SectionBaseX,SectionBaseY);
	ULandscapeHeightfieldCollisionComponent* CollisionComponent = LandscapeActor->XYtoCollisionComponentMap.FindRef(ComponentKey);

	INT CollisionSubsectionSizeVerts = ((SubsectionSizeQuads+1)>>CollisionMipLevel);
	INT CollisionSubsectionSizeQuads = CollisionSubsectionSizeVerts-1;
	INT CollisionSizeVerts = NumSubsections*CollisionSubsectionSizeQuads+1;

	WORD* CollisionHeightData = NULL;
	UBOOL CreatedNew = FALSE;
	if( CollisionComponent )
	{
		CollisionComponent->Modify();
		if( bUpdateBounds )
		{
			CollisionComponent->CachedBoxSphereBounds = CachedBoxSphereBounds;	
			CollisionComponent->ConditionalUpdateTransform();
		}

		CollisionHeightData = (WORD*)CollisionComponent->CollisionHeightData.Lock(LOCK_READ_WRITE);
	}
	else
	{
		ComponentX1 = 0;
		ComponentY1 = 0;
		ComponentX2 = MAXINT;
		ComponentY2 = MAXINT;

		CollisionComponent = ConstructObject<ULandscapeHeightfieldCollisionComponent>(ULandscapeHeightfieldCollisionComponent::StaticClass(), Proxy,NAME_None,RF_Transactional);
		Proxy->CollisionComponents.AddItem(CollisionComponent);
		LandscapeActor->XYtoCollisionComponentMap.Set(ComponentKey, CollisionComponent);

		CollisionComponent->SectionBaseX = SectionBaseX;
		CollisionComponent->SectionBaseY = SectionBaseY;
		CollisionComponent->CollisionSizeQuads = CollisionSubsectionSizeQuads * NumSubsections;
		CollisionComponent->CollisionScale = (FLOAT)(ComponentSizeQuads+1) / (FLOAT)(CollisionComponent->CollisionSizeQuads+1);
		CollisionComponent->CachedBoxSphereBounds = CachedBoxSphereBounds;
		CreatedNew = TRUE;

		// Reallocate raw collision data
		CollisionComponent->CollisionHeightData.Lock(LOCK_READ_WRITE);
		CollisionHeightData = (WORD*)CollisionComponent->CollisionHeightData.Realloc(Square(CollisionSizeVerts));
		appMemzero( CollisionHeightData,sizeof(WORD)*Square(CollisionSizeVerts));
	}

	INT HeightmapSizeU = HeightmapTexture->SizeX;
	INT HeightmapSizeV = HeightmapTexture->SizeY;
	INT MipSizeU = HeightmapSizeU >> CollisionMipLevel;
	INT MipSizeV = HeightmapSizeV >> CollisionMipLevel;

	// Ratio to convert update region coordinate to collision mip coordinates
	FLOAT CollisionQuadRatio = (FLOAT)CollisionSubsectionSizeQuads / (FLOAT)SubsectionSizeQuads;

	// XY offset into heightmap mip data
	INT HeightmapOffsetX = appRound(HeightmapScaleBias.Z * (FLOAT)HeightmapSizeU) >> CollisionMipLevel;
	INT HeightmapOffsetY = appRound(HeightmapScaleBias.W * (FLOAT)HeightmapSizeV) >> CollisionMipLevel;

	for( INT SubsectionY = 0;SubsectionY < NumSubsections;SubsectionY++ )
	{
		// Check if subsection is fully above or below the area we are interested in
		if( (ComponentY2 < SubsectionSizeQuads*SubsectionY) ||		// above
			(ComponentY1 > SubsectionSizeQuads*(SubsectionY+1)) )	// below
		{
			continue;
		}

		for( INT SubsectionX = 0;SubsectionX < NumSubsections;SubsectionX++ )
		{
			// Check if subsection is fully to the left or right of the area we are interested in
			if( (ComponentX2 < SubsectionSizeQuads*SubsectionX) ||		// left
				(ComponentX1 > SubsectionSizeQuads*(SubsectionX+1)) )	// right
			{
				continue;
			}

			// Area to update in subsection coordinates
			INT SubX1 = ComponentX1 - SubsectionSizeQuads*SubsectionX;
			INT SubY1 = ComponentY1 - SubsectionSizeQuads*SubsectionY;
			INT SubX2 = ComponentX2 - SubsectionSizeQuads*SubsectionX;
			INT SubY2 = ComponentY2 - SubsectionSizeQuads*SubsectionY;

			// Area to update in collision mip level coords
			INT CollisionSubX1 = appFloor( (FLOAT)SubX1 * CollisionQuadRatio );
			INT CollisionSubY1 = appFloor( (FLOAT)SubY1 * CollisionQuadRatio );
			INT CollisionSubX2 = appCeil(  (FLOAT)SubX2 * CollisionQuadRatio );
			INT CollisionSubY2 = appCeil(  (FLOAT)SubY2 * CollisionQuadRatio );

			// Clamp area to update
			INT VertX1 = Clamp<INT>(CollisionSubX1, 0, CollisionSubsectionSizeQuads);
			INT VertY1 = Clamp<INT>(CollisionSubY1, 0, CollisionSubsectionSizeQuads);
			INT VertX2 = Clamp<INT>(CollisionSubX2, 0, CollisionSubsectionSizeQuads);
			INT VertY2 = Clamp<INT>(CollisionSubY2, 0, CollisionSubsectionSizeQuads);

			for( INT VertY=VertY1;VertY<=VertY2;VertY++ )
			{
				for( INT VertX=VertX1;VertX<=VertX2;VertX++ )
				{
					// X/Y of the vertex we're looking indexed into the texture data
					INT TexX = HeightmapOffsetX + CollisionSubsectionSizeVerts * SubsectionX + VertX;
					INT TexY = HeightmapOffsetY + CollisionSubsectionSizeVerts * SubsectionY + VertY;
					FColor& TexData = HeightmapTextureMipData[ TexX + TexY * MipSizeU ];
			
					// this uses Quads as we don't want the duplicated vertices
					INT CompVertX = CollisionSubsectionSizeQuads * SubsectionX + VertX;
					INT CompVertY = CollisionSubsectionSizeQuads * SubsectionY + VertY;

					// Copy collision data
					WORD& CollisionHeight = CollisionHeightData[CompVertX+CompVertY*CollisionSizeVerts];
					WORD NewHeight = TexData.R<<8 | TexData.G;
					
					CollisionHeight = NewHeight;
				}
			}
		}
	}
	CollisionComponent->CollisionHeightData.Unlock();

	// If we updated an existing component, we need to reinitialize the physics
	if( !CreatedNew )
	{
		CollisionComponent->RecreateHeightfield();
	}
}

/**
* Generate mipmaps for height and tangent data.
* @param HeightmapTextureMipData - array of pointers to the locked mip data. 
*           This should only include the mips that are generated directly from this component's data
*           ie where each subsection has at least 2 vertices.
* @param ComponentX1 - region of texture to update in component space
* @param ComponentY1 - region of texture to update in component space
* @param ComponentX2 (optional) - region of texture to update in component space
* @param ComponentY2 (optional) - region of texture to update in component space
* @param TextureDataInfo - FLandscapeTextureDataInfo pointer, to notify of the mip data region updated.
*/
void ULandscapeComponent::GenerateHeightmapMips( TArray<FColor*>& HeightmapTextureMipData, INT ComponentX1/*=0*/, INT ComponentY1/*=0*/, INT ComponentX2/*=MAXINT*/, INT ComponentY2/*=MAXINT*/, struct FLandscapeTextureDataInfo* TextureDataInfo/*=NULL*/ )
{
	if( ComponentX2==MAXINT )
	{
		ComponentX2 = ComponentSizeQuads;
	}
	if( ComponentY2==MAXINT )
	{
		ComponentY2 = ComponentSizeQuads;
	}

	//ALandscape* LandscapeActor = GetLandscapeActor();
	INT HeightmapSizeU = HeightmapTexture->SizeX;
	INT HeightmapSizeV = HeightmapTexture->SizeY;

	INT HeightmapOffsetX = appRound(HeightmapScaleBias.Z * (FLOAT)HeightmapSizeU);
	INT HeightmapOffsetY = appRound(HeightmapScaleBias.W * (FLOAT)HeightmapSizeV);

	for( INT SubsectionY = 0;SubsectionY < NumSubsections;SubsectionY++ )
	{
		// Check if subsection is fully above or below the area we are interested in
		if( (ComponentY2 < SubsectionSizeQuads*SubsectionY) ||		// above
			(ComponentY1 > SubsectionSizeQuads*(SubsectionY+1)) )	// below
		{
			continue;
		}

		for( INT SubsectionX = 0;SubsectionX < NumSubsections;SubsectionX++ )
		{
			// Check if subsection is fully to the left or right of the area we are interested in
			if( (ComponentX2 < SubsectionSizeQuads*SubsectionX) ||		// left
				(ComponentX1 > SubsectionSizeQuads*(SubsectionX+1)) )	// right
			{
				continue;
			}

			// Area to update in previous mip level coords
			INT PrevMipSubX1 = ComponentX1 - SubsectionSizeQuads*SubsectionX;
			INT PrevMipSubY1 = ComponentY1 - SubsectionSizeQuads*SubsectionY;
			INT PrevMipSubX2 = ComponentX2 - SubsectionSizeQuads*SubsectionX;
			INT PrevMipSubY2 = ComponentY2 - SubsectionSizeQuads*SubsectionY;

			INT PrevMipSubsectionSizeQuads = SubsectionSizeQuads;
			FLOAT InvPrevMipSubsectionSizeQuads = 1.f / (FLOAT)SubsectionSizeQuads;

			INT PrevMipSizeU = HeightmapSizeU;
			INT PrevMipSizeV = HeightmapSizeV;

			INT PrevMipHeightmapOffsetX = HeightmapOffsetX;
			INT PrevMipHeightmapOffsetY = HeightmapOffsetY;

			for( INT Mip=1;Mip<HeightmapTextureMipData.Num();Mip++ )
			{
				INT MipSizeU = HeightmapSizeU >> Mip;
				INT MipSizeV = HeightmapSizeV >> Mip;

				INT MipSubsectionSizeQuads = ((SubsectionSizeQuads+1)>>Mip)-1;
				FLOAT InvMipSubsectionSizeQuads = 1.f / (FLOAT)MipSubsectionSizeQuads;

				INT MipHeightmapOffsetX = HeightmapOffsetX>>Mip;
				INT MipHeightmapOffsetY = HeightmapOffsetY>>Mip;

				// Area to update in current mip level coords
				INT MipSubX1 = appFloor( (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubX1 * InvPrevMipSubsectionSizeQuads );
				INT MipSubY1 = appFloor( (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubY1 * InvPrevMipSubsectionSizeQuads );
				INT MipSubX2 = appCeil(  (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubX2 * InvPrevMipSubsectionSizeQuads );
				INT MipSubY2 = appCeil(  (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubY2 * InvPrevMipSubsectionSizeQuads );

				// Clamp area to update
				INT VertX1 = Clamp<INT>(MipSubX1, 0, MipSubsectionSizeQuads);
				INT VertY1 = Clamp<INT>(MipSubY1, 0, MipSubsectionSizeQuads);
				INT VertX2 = Clamp<INT>(MipSubX2, 0, MipSubsectionSizeQuads);
				INT VertY2 = Clamp<INT>(MipSubY2, 0, MipSubsectionSizeQuads);

				for( INT VertY=VertY1;VertY<=VertY2;VertY++ )
				{
					for( INT VertX=VertX1;VertX<=VertX2;VertX++ )
					{
						// Convert VertX/Y into previous mip's coords
						FLOAT PrevMipVertX = (FLOAT)PrevMipSubsectionSizeQuads * (FLOAT)VertX * InvMipSubsectionSizeQuads;
						FLOAT PrevMipVertY = (FLOAT)PrevMipSubsectionSizeQuads * (FLOAT)VertY * InvMipSubsectionSizeQuads;

#if 0
						// Validate that the vertex we skip wouldn't use the updated data in the parent mip.
						// Note this validation is doesn't do anything unless you change the VertY/VertX loops 
						// above to process all verts from 0 .. MipSubsectionSizeQuads.
						if( VertX < VertX1 || VertX > VertX2 )
						{
							check( appCeil(PrevMipVertX) < PrevMipSubX1 || appFloor(PrevMipVertX) > PrevMipSubX2 );
							continue;
						}

						if( VertY < VertY1 || VertY > VertY2 )
						{
							check( appCeil(PrevMipVertY) < PrevMipSubY1 || appFloor(PrevMipVertY) > PrevMipSubY2 );
							continue;
						}
#endif

						// X/Y of the vertex we're looking indexed into the texture data
						INT TexX = (MipHeightmapOffsetX) + (MipSubsectionSizeQuads+1) * SubsectionX + VertX;
						INT TexY = (MipHeightmapOffsetY) + (MipSubsectionSizeQuads+1) * SubsectionY + VertY;

						FLOAT fPrevMipTexX = (FLOAT)(PrevMipHeightmapOffsetX) + (FLOAT)((PrevMipSubsectionSizeQuads+1) * SubsectionX) + PrevMipVertX;
						FLOAT fPrevMipTexY = (FLOAT)(PrevMipHeightmapOffsetY) + (FLOAT)((PrevMipSubsectionSizeQuads+1) * SubsectionY) + PrevMipVertY;

						INT PrevMipTexX = appFloor(fPrevMipTexX);
						FLOAT fPrevMipTexFracX = appFractional(fPrevMipTexX);
						INT PrevMipTexY = appFloor(fPrevMipTexY);
						FLOAT fPrevMipTexFracY = appFractional(fPrevMipTexY);

						checkSlow( TexX >= 0 && TexX < MipSizeU );
						checkSlow( TexY >= 0 && TexY < MipSizeV );
						checkSlow( PrevMipTexX >= 0 && PrevMipTexX < PrevMipSizeU );
						checkSlow( PrevMipTexY >= 0 && PrevMipTexY < PrevMipSizeV );

						INT PrevMipTexX1 = Min<INT>( PrevMipTexX+1, PrevMipSizeU-1 );
						INT PrevMipTexY1 = Min<INT>( PrevMipTexY+1, PrevMipSizeV-1 );

						FColor* TexData = &(HeightmapTextureMipData(Mip))[ TexX + TexY * MipSizeU ];
						FColor *PreMipTexData00 = &(HeightmapTextureMipData(Mip-1))[ PrevMipTexX + PrevMipTexY * PrevMipSizeU ];
						FColor *PreMipTexData01 = &(HeightmapTextureMipData(Mip-1))[ PrevMipTexX + PrevMipTexY1 * PrevMipSizeU ];
						FColor *PreMipTexData10 = &(HeightmapTextureMipData(Mip-1))[ PrevMipTexX1 + PrevMipTexY * PrevMipSizeU ];
						FColor *PreMipTexData11 = &(HeightmapTextureMipData(Mip-1))[ PrevMipTexX1 + PrevMipTexY1 * PrevMipSizeU ];

						// Lerp height values
						WORD PrevMipHeightValue00 = PreMipTexData00->R << 8 | PreMipTexData00->G;
						WORD PrevMipHeightValue01 = PreMipTexData01->R << 8 | PreMipTexData01->G;
						WORD PrevMipHeightValue10 = PreMipTexData10->R << 8 | PreMipTexData10->G;
						WORD PrevMipHeightValue11 = PreMipTexData11->R << 8 | PreMipTexData11->G;
						WORD HeightValue = appRound(
							Lerp(
								Lerp( (FLOAT)PrevMipHeightValue00, (FLOAT)PrevMipHeightValue10, fPrevMipTexFracX),
								Lerp( (FLOAT)PrevMipHeightValue01, (FLOAT)PrevMipHeightValue11, fPrevMipTexFracX),
							fPrevMipTexFracY) );

						TexData->R = HeightValue >> 8;
						TexData->G = HeightValue & 255;

						// Lerp tangents
						TexData->B = appRound(
							Lerp(
							Lerp( (FLOAT)PreMipTexData00->B, (FLOAT)PreMipTexData10->B, fPrevMipTexFracX),
							Lerp( (FLOAT)PreMipTexData01->B, (FLOAT)PreMipTexData11->B, fPrevMipTexFracX),
							fPrevMipTexFracY) );

						TexData->A = appRound(
							Lerp(
							Lerp( (FLOAT)PreMipTexData00->A, (FLOAT)PreMipTexData10->A, fPrevMipTexFracX),
							Lerp( (FLOAT)PreMipTexData01->A, (FLOAT)PreMipTexData11->A, fPrevMipTexFracX),
							fPrevMipTexFracY) );
					}
				}

				// Record the areas we updated
				if( TextureDataInfo )
				{
					INT TexX1 = (MipHeightmapOffsetX) + (MipSubsectionSizeQuads+1) * SubsectionX + VertX1;
					INT TexY1 = (MipHeightmapOffsetY) + (MipSubsectionSizeQuads+1) * SubsectionY + VertY1;
					INT TexX2 = (MipHeightmapOffsetX) + (MipSubsectionSizeQuads+1) * SubsectionX + VertX2;
					INT TexY2 = (MipHeightmapOffsetY) + (MipSubsectionSizeQuads+1) * SubsectionY + VertY2;
					TextureDataInfo->AddMipUpdateRegion(Mip,TexX1,TexY1,TexX2,TexY2);
				}

				// Copy current mip values to prev as we move to the next mip.
				PrevMipSubsectionSizeQuads = MipSubsectionSizeQuads;
				InvPrevMipSubsectionSizeQuads = InvMipSubsectionSizeQuads;

				PrevMipSizeU = MipSizeU;
				PrevMipSizeV = MipSizeV;

				PrevMipHeightmapOffsetX = MipHeightmapOffsetX;
				PrevMipHeightmapOffsetY = MipHeightmapOffsetY;

				// Use this mip's area as we move to the next mip
				PrevMipSubX1 = MipSubX1;
				PrevMipSubY1 = MipSubY1;
				PrevMipSubX2 = MipSubX2;
				PrevMipSubY2 = MipSubY2;
			}
		}
	}
}

void ULandscapeComponent::CreateEmptyWeightmapMips(UTexture2D* WeightmapTexture)
{
	// Remove any existing mips.
	WeightmapTexture->Mips.Remove(1, WeightmapTexture->Mips.Num()-1);

	INT WeightmapSizeU = WeightmapTexture->SizeX;
	INT WeightmapSizeV = WeightmapTexture->SizeY;

	INT Mip = 1;
	for(;;)
	{
		INT MipSizeU = Max<INT>(WeightmapSizeU >> Mip,1);
		INT MipSizeV = Max<INT>(WeightmapSizeV >> Mip,1);

		// Allocate the mipmap
		FTexture2DMipMap* WeightMipMap = new(WeightmapTexture->Mips) FTexture2DMipMap;
		WeightMipMap->SizeX = MipSizeU;
		WeightMipMap->SizeY = MipSizeV;
		WeightMipMap->Data.Lock(LOCK_READ_WRITE);
		WeightMipMap->Data.Realloc(MipSizeU*MipSizeV*sizeof(FColor));
		WeightMipMap->Data.Unlock();

		if( MipSizeU == 1 && MipSizeV == 1 )
		{
			break;
		}

		Mip++;
	}
}

void ULandscapeComponent::GenerateWeightmapMips(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* WeightmapTexture, FColor* BaseMipData)
{
	// Remove any existing mips.
	WeightmapTexture->Mips.Remove(1, WeightmapTexture->Mips.Num()-1);

	// Stores pointers to the locked mip data
	TArray<FColor*> WeightmapTextureMipData;

	// Add the first mip's data
	WeightmapTextureMipData.AddItem( BaseMipData );

	INT WeightmapSizeU = WeightmapTexture->SizeX;
	INT WeightmapSizeV = WeightmapTexture->SizeY;

	INT Mip = 1;
	for(;;)
	{
		INT MipSizeU = Max<INT>(WeightmapSizeU >> Mip,1);
		INT MipSizeV = Max<INT>(WeightmapSizeV >> Mip,1);

		// Create the mipmap
		FTexture2DMipMap* WeightMipMap = new(WeightmapTexture->Mips) FTexture2DMipMap;
		WeightMipMap->SizeX = MipSizeU;
		WeightMipMap->SizeY = MipSizeV;
		WeightMipMap->Data.Lock(LOCK_READ_WRITE);
		WeightmapTextureMipData.AddItem( (FColor*)WeightMipMap->Data.Realloc(MipSizeU*MipSizeV*sizeof(FColor)) );

		if( MipSizeU == 1 && MipSizeV == 1 )
		{
			break;
		}

		Mip++;
	}

	// Update the newly created mips
	UpdateWeightmapMips( InNumSubsections, InSubsectionSizeQuads, WeightmapTexture, WeightmapTextureMipData );

	// Unlock all the new mips, but not the base mip's data
	for( INT i=1;i<WeightmapTextureMipData.Num();i++ )
	{
		WeightmapTexture->Mips(i).Data.Unlock();
	}
}

void ULandscapeComponent::UpdateWeightmapMips(INT InNumSubsections, INT InSubsectionSizeQuads, UTexture2D* WeightmapTexture, TArray<FColor*>& WeightmapTextureMipData, INT ComponentX1/*=0*/, INT ComponentY1/*=0*/, INT ComponentX2/*=MAXINT*/, INT ComponentY2/*=MAXINT*/, struct FLandscapeTextureDataInfo* TextureDataInfo/*=NULL*/)
{
	INT WeightmapSizeU = WeightmapTexture->SizeX;
	INT WeightmapSizeV = WeightmapTexture->SizeY;

	// Find the maximum mip where each texel's data comes from just one subsection.
	INT MaxWholeSubsectionMip = 1;
	INT Mip=1;
	for(;;)
	{
		INT MipSubsectionSizeQuads = ((InSubsectionSizeQuads+1)>>Mip)-1;

		INT MipSizeU = Max<INT>(WeightmapSizeU >> Mip,1);
		INT MipSizeV = Max<INT>(WeightmapSizeV >> Mip,1);

		// Mip must represent at least one quad to store valid weight data
		if( MipSubsectionSizeQuads > 0 )
		{
			MaxWholeSubsectionMip = Mip;
		}
		else
		{
			break;
		}

		Mip++;
	}

	// Update the mip where each texel's data comes from just one subsection.
	for( INT SubsectionY = 0;SubsectionY < InNumSubsections;SubsectionY++ )
	{
		// Check if subsection is fully above or below the area we are interested in
		if( (ComponentY2 < InSubsectionSizeQuads*SubsectionY) ||	// above
			(ComponentY1 > InSubsectionSizeQuads*(SubsectionY+1)) )	// below
		{
			continue;
		}

		for( INT SubsectionX = 0;SubsectionX < InNumSubsections;SubsectionX++ )
		{
			// Check if subsection is fully to the left or right of the area we are interested in
			if( (ComponentX2 < InSubsectionSizeQuads*SubsectionX) ||	// left
				(ComponentX1 > InSubsectionSizeQuads*(SubsectionX+1)) )	// right
			{
				continue;
			}

			// Area to update in previous mip level coords
			INT PrevMipSubX1 = ComponentX1 - InSubsectionSizeQuads*SubsectionX;
			INT PrevMipSubY1 = ComponentY1 - InSubsectionSizeQuads*SubsectionY;
			INT PrevMipSubX2 = ComponentX2 - InSubsectionSizeQuads*SubsectionX;
			INT PrevMipSubY2 = ComponentY2 - InSubsectionSizeQuads*SubsectionY;

			INT PrevMipSubsectionSizeQuads = InSubsectionSizeQuads;
			FLOAT InvPrevMipSubsectionSizeQuads = 1.f / (FLOAT)InSubsectionSizeQuads;

			INT PrevMipSizeU = WeightmapSizeU;
			INT PrevMipSizeV = WeightmapSizeV;

			for( INT Mip=1;Mip<=MaxWholeSubsectionMip;Mip++ )
			{
				INT MipSizeU = WeightmapSizeU >> Mip;
				INT MipSizeV = WeightmapSizeV >> Mip;

				INT MipSubsectionSizeQuads = ((InSubsectionSizeQuads+1)>>Mip)-1;
				FLOAT InvMipSubsectionSizeQuads = 1.f / (FLOAT)MipSubsectionSizeQuads;

				// Area to update in current mip level coords
				INT MipSubX1 = appFloor( (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubX1 * InvPrevMipSubsectionSizeQuads );
				INT MipSubY1 = appFloor( (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubY1 * InvPrevMipSubsectionSizeQuads );
				INT MipSubX2 = appCeil(  (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubX2 * InvPrevMipSubsectionSizeQuads );
				INT MipSubY2 = appCeil(  (FLOAT)MipSubsectionSizeQuads * (FLOAT)PrevMipSubY2 * InvPrevMipSubsectionSizeQuads );

				// Clamp area to update
				INT VertX1 = Clamp<INT>(MipSubX1, 0, MipSubsectionSizeQuads);
				INT VertY1 = Clamp<INT>(MipSubY1, 0, MipSubsectionSizeQuads);
				INT VertX2 = Clamp<INT>(MipSubX2, 0, MipSubsectionSizeQuads);
				INT VertY2 = Clamp<INT>(MipSubY2, 0, MipSubsectionSizeQuads);

				for( INT VertY=VertY1;VertY<=VertY2;VertY++ )
				{
					for( INT VertX=VertX1;VertX<=VertX2;VertX++ )
					{
						// Convert VertX/Y into previous mip's coords
						FLOAT PrevMipVertX = (FLOAT)PrevMipSubsectionSizeQuads * (FLOAT)VertX * InvMipSubsectionSizeQuads;
						FLOAT PrevMipVertY = (FLOAT)PrevMipSubsectionSizeQuads * (FLOAT)VertY * InvMipSubsectionSizeQuads;

						// X/Y of the vertex we're looking indexed into the texture data
						INT TexX = (MipSubsectionSizeQuads+1) * SubsectionX + VertX;
						INT TexY = (MipSubsectionSizeQuads+1) * SubsectionY + VertY;

						FLOAT fPrevMipTexX = (FLOAT)((PrevMipSubsectionSizeQuads+1) * SubsectionX) + PrevMipVertX;
						FLOAT fPrevMipTexY = (FLOAT)((PrevMipSubsectionSizeQuads+1) * SubsectionY) + PrevMipVertY;

						INT PrevMipTexX = appFloor(fPrevMipTexX);
						FLOAT fPrevMipTexFracX = appFractional(fPrevMipTexX);
						INT PrevMipTexY = appFloor(fPrevMipTexY);
						FLOAT fPrevMipTexFracY = appFractional(fPrevMipTexY);

						check( TexX >= 0 && TexX < MipSizeU );
						check( TexY >= 0 && TexY < MipSizeV );
						check( PrevMipTexX >= 0 && PrevMipTexX < PrevMipSizeU );
						check( PrevMipTexY >= 0 && PrevMipTexY < PrevMipSizeV );

						INT PrevMipTexX1 = Min<INT>( PrevMipTexX+1, PrevMipSizeU-1 );
						INT PrevMipTexY1 = Min<INT>( PrevMipTexY+1, PrevMipSizeV-1 );

						FColor* TexData = &(WeightmapTextureMipData(Mip))[ TexX + TexY * MipSizeU ];
						FColor *PreMipTexData00 = &(WeightmapTextureMipData(Mip-1))[ PrevMipTexX + PrevMipTexY * PrevMipSizeU ];
						FColor *PreMipTexData01 = &(WeightmapTextureMipData(Mip-1))[ PrevMipTexX + PrevMipTexY1 * PrevMipSizeU ];
						FColor *PreMipTexData10 = &(WeightmapTextureMipData(Mip-1))[ PrevMipTexX1 + PrevMipTexY * PrevMipSizeU ];
						FColor *PreMipTexData11 = &(WeightmapTextureMipData(Mip-1))[ PrevMipTexX1 + PrevMipTexY1 * PrevMipSizeU ];

						// Lerp weightmap data
						TexData->R = appRound(
							Lerp(
							Lerp( (FLOAT)PreMipTexData00->R, (FLOAT)PreMipTexData10->R, fPrevMipTexFracX),
							Lerp( (FLOAT)PreMipTexData01->R, (FLOAT)PreMipTexData11->R, fPrevMipTexFracX),
							fPrevMipTexFracY) );
						TexData->G = appRound(
							Lerp(
							Lerp( (FLOAT)PreMipTexData00->G, (FLOAT)PreMipTexData10->G, fPrevMipTexFracX),
							Lerp( (FLOAT)PreMipTexData01->G, (FLOAT)PreMipTexData11->G, fPrevMipTexFracX),
							fPrevMipTexFracY) );
						TexData->B = appRound(
							Lerp(
							Lerp( (FLOAT)PreMipTexData00->B, (FLOAT)PreMipTexData10->B, fPrevMipTexFracX),
							Lerp( (FLOAT)PreMipTexData01->B, (FLOAT)PreMipTexData11->B, fPrevMipTexFracX),
							fPrevMipTexFracY) );
						TexData->A = appRound(
							Lerp(
							Lerp( (FLOAT)PreMipTexData00->A, (FLOAT)PreMipTexData10->A, fPrevMipTexFracX),
							Lerp( (FLOAT)PreMipTexData01->A, (FLOAT)PreMipTexData11->A, fPrevMipTexFracX),
							fPrevMipTexFracY) );
					}
				}

				// Record the areas we updated
				if( TextureDataInfo )
				{
					INT TexX1 = (MipSubsectionSizeQuads+1) * SubsectionX + VertX1;
					INT TexY1 = (MipSubsectionSizeQuads+1) * SubsectionY + VertY1;
					INT TexX2 = (MipSubsectionSizeQuads+1) * SubsectionX + VertX2;
					INT TexY2 = (MipSubsectionSizeQuads+1) * SubsectionY + VertY2;
					TextureDataInfo->AddMipUpdateRegion(Mip,TexX1,TexY1,TexX2,TexY2);
				}

				// Copy current mip values to prev as we move to the next mip.
				PrevMipSubsectionSizeQuads = MipSubsectionSizeQuads;
				InvPrevMipSubsectionSizeQuads = InvMipSubsectionSizeQuads;

				PrevMipSizeU = MipSizeU;
				PrevMipSizeV = MipSizeV;

				// Use this mip's area as we move to the next mip
				PrevMipSubX1 = MipSubX1;
				PrevMipSubY1 = MipSubY1;
				PrevMipSubX2 = MipSubX2;
				PrevMipSubY2 = MipSubY2;
			}
		}
	}

	// Handle mips that have texels from multiple subsections
	Mip=1;
	for(;;)
	{
		INT MipSubsectionSizeQuads = ((InSubsectionSizeQuads+1)>>Mip)-1;

		INT MipSizeU = Max<INT>(WeightmapSizeU >> Mip,1);
		INT MipSizeV = Max<INT>(WeightmapSizeV >> Mip,1);

		// Mip must represent at least one quad to store valid weight data
		if( MipSubsectionSizeQuads <= 0 )
		{
			INT PrevMipSizeU = WeightmapSizeU >> (Mip-1);
			INT PrevMipSizeV = WeightmapSizeV >> (Mip-1);

			// not valid weight data, so just average the texels of the previous mip.
			for( INT Y = 0;Y < MipSizeV;Y++ )
			{
				for( INT X = 0;X < MipSizeU;X++ )
				{
					FColor* TexData = &(WeightmapTextureMipData(Mip))[ X + Y * MipSizeU ];

					FColor *PreMipTexData00 = &(WeightmapTextureMipData(Mip-1))[ (X*2+0) + (Y*2+0)  * PrevMipSizeU ];
					FColor *PreMipTexData01 = &(WeightmapTextureMipData(Mip-1))[ (X*2+0) + (Y*2+1)  * PrevMipSizeU ];
					FColor *PreMipTexData10 = &(WeightmapTextureMipData(Mip-1))[ (X*2+1) + (Y*2+0)  * PrevMipSizeU ];
					FColor *PreMipTexData11 = &(WeightmapTextureMipData(Mip-1))[ (X*2+1) + (Y*2+1)  * PrevMipSizeU ];

					TexData->R = (((INT)PreMipTexData00->R + (INT)PreMipTexData01->R + (INT)PreMipTexData10->R + (INT)PreMipTexData11->R) >> 2);
					TexData->G = (((INT)PreMipTexData00->G + (INT)PreMipTexData01->G + (INT)PreMipTexData10->G + (INT)PreMipTexData11->G) >> 2);
					TexData->B = (((INT)PreMipTexData00->B + (INT)PreMipTexData01->B + (INT)PreMipTexData10->B + (INT)PreMipTexData11->B) >> 2);
					TexData->A = (((INT)PreMipTexData00->A + (INT)PreMipTexData01->A + (INT)PreMipTexData10->A + (INT)PreMipTexData11->A) >> 2);
				}
			}

			if( TextureDataInfo )
			{
				// These mip sizes are small enough that we may as well just update the whole mip.
				TextureDataInfo->AddMipUpdateRegion(Mip,0,0,MipSizeU-1,MipSizeV-1);
			}
		}

		if( MipSizeU == 1 && MipSizeV == 1 )
		{
			break;
		}

		Mip++;
	}
}

//
// ALandscape
//

#define MAX_LANDSCAPE_SUBSECTIONS 3

void ALandscape::GetComponentsInRegion(INT X1, INT Y1, INT X2, INT Y2, TSet<ULandscapeComponent*>& OutComponents)
{
	if (ComponentSizeQuads <= 0)
	{
		return;
	}
	// Find component range for this block of data
	INT ComponentIndexX1 = (X1-1 >= 0) ? (X1-1) / ComponentSizeQuads : (X1) / ComponentSizeQuads - 1;	// -1 because we need to pick up vertices shared between components
	INT ComponentIndexY1 = (Y1-1 >= 0) ? (Y1-1) / ComponentSizeQuads : (Y1) / ComponentSizeQuads - 1;
	INT ComponentIndexX2 = (X2 >= 0) ? X2 / ComponentSizeQuads : (X2+1) / ComponentSizeQuads - 1;
	INT ComponentIndexY2 = (Y2 >= 0) ? Y2 / ComponentSizeQuads : (Y2+1) / ComponentSizeQuads - 1;

	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{		
			ULandscapeComponent* Component = XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads));
			if( Component )
			{
				OutComponents.Add(Component);
			}
		}
	}
}

UBOOL ALandscape::ImportFromOldTerrain(ATerrain* OldTerrain)
{
	if( !OldTerrain )
	{
		return FALSE;
	}

	// Landscape doesn't support rotation...
	if( !OldTerrain->Rotation.IsZero() )
	{
		appMsgf(AMT_OK,TEXT("The Terrain actor has Non-zero rotation, and Landscape doesn't support rotation"), *OldTerrain->GetName());
		return FALSE;
	}

	// Work out how many subsections we need.
	// Until we've got a SubsectionSizeQuads that's one less than a power of 2
	// Keep adding new sections, or until we run out of 
	ComponentSizeQuads = OldTerrain->MaxComponentSize;
	NumSubsections = 1;
	SubsectionSizeQuads = ComponentSizeQuads;
	while( ((SubsectionSizeQuads) & (SubsectionSizeQuads+1)) != 0 || NumSubsections*SubsectionSizeQuads != ComponentSizeQuads )
	{
		if( NumSubsections > MAX_LANDSCAPE_SUBSECTIONS || ComponentSizeQuads / NumSubsections < 1 )
		{
			appMsgf(AMT_OK,TEXT("The Terrain actor %s's MaxComponentSize must be an 1x, 2x or 3x multiple of one less than a power of two"), *OldTerrain->GetName());
			return FALSE;
		}

		// try adding another subsection.
		NumSubsections++;
		SubsectionSizeQuads = ComponentSizeQuads / NumSubsections;
	}

	// Should check after changing NumSubsections
	if( NumSubsections > MAX_LANDSCAPE_SUBSECTIONS || ComponentSizeQuads / NumSubsections < 1 )
	{
		appMsgf(AMT_OK,TEXT("The Terrain actor %s's MaxComponentSize must be an 1x, 2x or 3x multiple of one less than a power of two"), *OldTerrain->GetName());
		return FALSE;
	}

	debugf(TEXT("%s: using ComponentSizeQuads=%d, NumSubsections=%d, SubsectionSizeQuads=%d"), *OldTerrain->GetName(), ComponentSizeQuads, NumSubsections, SubsectionSizeQuads);

	// Validate old terrain.
	if( OldTerrain->NumPatchesX % OldTerrain->MaxComponentSize != 0 ||
		OldTerrain->NumPatchesX % OldTerrain->MaxComponentSize != 0 )
	{
		appMsgf(AMT_OK,TEXT("The Terrain actor %s's NumPatchesX/Y must be multiples of MaxComponentSize"), *OldTerrain->GetName());
		return FALSE;
	}
	if( OldTerrain->MaxTesselationLevel > 1 )
	{
		appMsgf(AMT_OK,TEXT("The Terrain actor %s's MaxTesselationLevel must be set to 1."), *OldTerrain->GetName());
		return FALSE;
	}

	GWarn->BeginSlowTask( *FString::Printf(TEXT("Converting terrain %s"), *OldTerrain->GetName()), TRUE);

	// Create and import terrain material
	UMaterial* LandscapeUMaterial = ConstructObject<UMaterial>(UMaterial::StaticClass(), GetOutermost(), FName(*FString::Printf(TEXT("%s_Material"),*GetName())), RF_Public|RF_Standalone);
	LandscapeMaterial = LandscapeUMaterial;

	TMap<EMaterialProperty, UMaterialExpressionTerrainLayerWeight*> MaterialPropertyLastLayerWeightExpressionMap;
	
	INT YOffset = 0;

	for( INT LayerIndex=0;LayerIndex<OldTerrain->Layers.Num();LayerIndex++ )
	{
		UTerrainLayerSetup*	Setup = OldTerrain->Layers(LayerIndex).Setup;
		if( Setup )
		{
			if( Setup->Materials.Num() == 1)
			{
				FTerrainFilteredMaterial& FilteredTerrainMaterial = Setup->Materials(0);
				if( FilteredTerrainMaterial.Material )
				{
					UMaterial* Material = Cast<UMaterial>(FilteredTerrainMaterial.Material->Material);
					if( Material == NULL )
					{
						debugf(TEXT("%s's Material is not a plain UMaterial, skipping..."), *FilteredTerrainMaterial.Material->GetName());
						continue;
					}

					FLandscapeLayerInfo Info(FName(*FString::Printf(TEXT("Layer%d"),LayerIndex)));
					LayerInfos.AddItem(Info);

					TArray<UMaterialExpression*> NewExpressions;
					TArray<UMaterialExpression*> NewComments;

					// Copy all the expression from the material to our new material
					UMaterialExpression::CopyMaterialExpressions(Material->Expressions, Material->EditorComments, LandscapeUMaterial, NewExpressions, NewComments);

					for( INT CommentIndex=0;CommentIndex<NewComments.Num();CommentIndex++ )
					{
						// Move comments
						UMaterialExpression* Comment = NewComments(CommentIndex);
						Comment->MaterialExpressionEditorX += 200;
						Comment->MaterialExpressionEditorY += YOffset;
					}

					UMaterialExpressionTerrainLayerCoords* LayerTexCoords = NULL;

					for( INT ExpressionIndex=0;ExpressionIndex<NewExpressions.Num();ExpressionIndex++ )
					{
						UMaterialExpression* Expression = NewExpressions(ExpressionIndex);

						// Move expressions
						Expression->MaterialExpressionEditorX += 200;
						Expression->MaterialExpressionEditorY += YOffset;

						// Fix up texture coordinates for this layer
						UMaterialExpressionTextureSample* TextureSampleExpression = Cast<UMaterialExpressionTextureSample>(Expression);
						if( TextureSampleExpression != NULL) // && TextureSampleExpression->Coordinates.Expression == NULL)
						{
							if( LayerTexCoords == NULL )
							{
								LayerTexCoords = ConstructObject<UMaterialExpressionTerrainLayerCoords>( UMaterialExpressionTerrainLayerCoords::StaticClass(), LandscapeUMaterial, NAME_None, RF_Transactional );
								LandscapeUMaterial->Expressions.AddItem(LayerTexCoords);
								LayerTexCoords->MaterialExpressionEditorX = Expression->MaterialExpressionEditorX + 120;
								LayerTexCoords->MaterialExpressionEditorY = Expression->MaterialExpressionEditorY + 48;
								check((INT)TMT_MAX == (INT)TCMT_MAX);
								//!! TODO make these parameters
								LayerTexCoords->MappingType		= FilteredTerrainMaterial.Material->MappingType;
								LayerTexCoords->MappingScale	= FilteredTerrainMaterial.Material->MappingScale;
								LayerTexCoords->MappingRotation = FilteredTerrainMaterial.Material->MappingRotation;
								LayerTexCoords->MappingPanU		= FilteredTerrainMaterial.Material->MappingPanU;
								LayerTexCoords->MappingPanV		= FilteredTerrainMaterial.Material->MappingPanV;
							}

							TextureSampleExpression->Coordinates.Expression = LayerTexCoords;
						}
					}

					INT NumPropertiesWeighted = 0;
					for( INT PropertyIndex=0;PropertyIndex < MP_MAX;PropertyIndex++ )
					{
						FExpressionInput* PropertyInput = Material->GetExpressionInputForProperty((EMaterialProperty)PropertyIndex);
						if( PropertyInput->Expression )
						{
							// Need to construct a new UMaterialExpressionTerrainLayerWeight to blend in this layer for this input.
							UMaterialExpressionTerrainLayerWeight* WeightExpression = ConstructObject<UMaterialExpressionTerrainLayerWeight>( UMaterialExpressionTerrainLayerWeight::StaticClass(), LandscapeUMaterial, NAME_None, RF_Transactional );
							LandscapeUMaterial->Expressions.AddItem(WeightExpression);
							WeightExpression->ConditionallyGenerateGUID(TRUE);

							WeightExpression->MaterialExpressionEditorX = 200 + 32 * NumPropertiesWeighted++;
							WeightExpression->MaterialExpressionEditorY = YOffset;
							YOffset += 64;

							// Connect the previous layer's weight blend for this material property as the Base, or NULL if there was none.
							WeightExpression->Base.Expression = MaterialPropertyLastLayerWeightExpressionMap.FindRef((EMaterialProperty)PropertyIndex);

							// Connect this layer to the Layer input.
							WeightExpression->Layer = *PropertyInput;

							// Remap the expression it points to, so we're working on the copy.
							INT ExpressionIndex = Material->Expressions.FindItemIndex(PropertyInput->Expression);
							check(ExpressionIndex != INDEX_NONE);					
							WeightExpression->Layer.Expression = NewExpressions(ExpressionIndex);
							WeightExpression->ParameterName = FName(*FString::Printf(TEXT("Layer%d"),LayerIndex));
							
							// Remember this weight expression as the last layer for this material property
							MaterialPropertyLastLayerWeightExpressionMap.Set((EMaterialProperty)PropertyIndex, WeightExpression);
						}
					}

					for( INT ExpressionIndex=0;ExpressionIndex<NewExpressions.Num();ExpressionIndex++ )
					{
						UMaterialExpression* Expression = NewExpressions(ExpressionIndex);

						if( Expression->MaterialExpressionEditorY > YOffset )
						{
							YOffset = Expression->MaterialExpressionEditorY;
						}
					}

					YOffset += 64;
				}
			}
			else
			{
				debugf(TEXT("%s is a multi-layer filtered material, skipping..."), *Setup->GetName());
			}
		}
	}

	// Assign all the material inputs
	for( INT PropertyIndex=0;PropertyIndex < MP_MAX;PropertyIndex++ )
	{
		UMaterialExpressionTerrainLayerWeight* WeightExpression = MaterialPropertyLastLayerWeightExpressionMap.FindRef((EMaterialProperty)PropertyIndex);
		if( WeightExpression )
		{
			FExpressionInput* PropertyInput = LandscapeUMaterial->GetExpressionInputForProperty((EMaterialProperty)PropertyIndex);
			check(PropertyInput);
			PropertyInput->Expression = WeightExpression;
		}
	}

	DrawScale = OldTerrain->DrawScale;
	DrawScale3D = OldTerrain->DrawScale3D;

	// import!
	INT VertsX = OldTerrain->NumPatchesX+1;
	INT VertsY = OldTerrain->NumPatchesY+1;

	// Copy height data
	WORD* HeightData = new WORD[VertsX*VertsY];
	for( INT Y=0;Y<VertsY;Y++ )
	{
		for( INT X=0;X<VertsX;X++ )
		{
			HeightData[Y*VertsX + X] = OldTerrain->Height(X,Y);
		}
	}

	TArray<FLandscapeLayerInfo> ImportLayerInfos;
	TArray<BYTE*> ImportAlphaDataPointers;

	// Copy over Alphamap data from old terrain
	for( INT LayerIndex=0;LayerIndex<OldTerrain->Layers.Num();LayerIndex++ )
	{
		INT AlphaMapIndex = OldTerrain->Layers(LayerIndex).AlphaMapIndex;

		if( AlphaMapIndex != -1 || LayerIndex == 0 )
		{
			BYTE* AlphaData = new BYTE[VertsX*VertsY];
			ImportAlphaDataPointers.AddItem(AlphaData);
			new(ImportLayerInfos) FLandscapeLayerInfo(FName(*FString::Printf(TEXT("Layer%d"),LayerIndex)));
			//ImportLayerNames.AddItem(FName(*FString::Printf(TEXT("Layer%d"),LayerIndex)));

			if( AlphaMapIndex == -1 || LayerIndex == 0 )
			{
				// First layer doesn't have an alphamap, as it's completely opaque.
				appMemset( AlphaData, 255, VertsX*VertsY );
			}
			else
			{
				check( OldTerrain->AlphaMaps(AlphaMapIndex).Data.Num() == VertsX * VertsY );
				appMemcpy(AlphaData, &OldTerrain->AlphaMaps(AlphaMapIndex).Data(0), VertsX * VertsY);
			}
		}
	}

	// import heightmap and weightmap
	Import(VertsX, VertsY, ComponentSizeQuads, NumSubsections, SubsectionSizeQuads, HeightData, ImportLayerInfos, &ImportAlphaDataPointers(0));

	delete[] HeightData;
	for( INT i=0;i<ImportAlphaDataPointers.Num();i++)
	{
		delete []ImportAlphaDataPointers(i);
	}
	ImportAlphaDataPointers.Empty();

	GWarn->EndSlowTask();

	return TRUE;
}

// A struct to remember where we have spare texture channels.
struct FWeightmapTextureAllocation
{
	INT X;
	INT Y;
	INT ChannelsInUse;
	UTexture2D* Texture;
	FColor* TextureData;

	FWeightmapTextureAllocation( INT InX, INT InY, INT InChannels, UTexture2D* InTexture, FColor* InTextureData )
		:	X(InX)
		,	Y(InY)
		,	ChannelsInUse(InChannels)
		,	Texture(InTexture)
		,	TextureData(InTextureData)
	{}
};

// A struct to hold the info about each texture chunk of the total heightmap
struct FHeightmapInfo
{
	INT HeightmapSizeU;
	INT HeightmapSizeV;
	UTexture2D* HeightmapTexture;
	TArray<FColor*> HeightmapTextureMipData;
};


#define HEIGHTDATA(X,Y) (HeightData[ Clamp<INT>(Y,0,VertsY) * VertsX + Clamp<INT>(X,0,VertsX) ])
void ALandscape::Import(INT VertsX, INT VertsY, INT InComponentSizeQuads, INT InNumSubsections, INT InSubsectionSizeQuads, WORD* HeightData, TArray<FLandscapeLayerInfo> ImportLayerInfos, BYTE* AlphaDataPointers[] )
{
	GWarn->BeginSlowTask( TEXT("Importing Landscape"), TRUE);

	ComponentSizeQuads = InComponentSizeQuads;
	NumSubsections = InNumSubsections;
	SubsectionSizeQuads = InSubsectionSizeQuads;

	MarkPackageDirty();

	INT NumPatchesX = (VertsX-1);
	INT NumPatchesY = (VertsY-1);

	INT NumSectionsX = NumPatchesX / ComponentSizeQuads;
	INT NumSectionsY = NumPatchesY / ComponentSizeQuads;

	LayerInfos = ImportLayerInfos;
	LandscapeComponents.Empty(NumSectionsX * NumSectionsY);

	for (INT Y = 0; Y < NumSectionsY; Y++)
	{
		for (INT X = 0; X < NumSectionsX; X++)
		{
			// The number of quads
			INT NumQuadsX = NumPatchesX;
			INT NumQuadsY = NumPatchesY;

			INT BaseX = X * ComponentSizeQuads;
			INT BaseY = Y * ComponentSizeQuads;

			ULandscapeComponent* LandscapeComponent = ConstructObject<ULandscapeComponent>(ULandscapeComponent::StaticClass(),this,NAME_None,RF_Transactional);
			LandscapeComponents.AddItem(LandscapeComponent);
			LandscapeComponent->Init(
				BaseX,BaseY,
				ComponentSizeQuads,
				NumSubsections,
				SubsectionSizeQuads
				);

#if 0
			// Propagate shadow/ lighting options from ATerrain to component.
			LandscapeComponent->CastShadow			= bCastShadow;
			LandscapeComponent->bCastDynamicShadow	= bCastDynamicShadow;
			LandscapeComponent->bForceDirectLightMap	= bForceDirectLightMap;
			LandscapeComponent->BlockRigidBody		= bBlockRigidBody;
			LandscapeComponent->bAcceptsDynamicLights	= bAcceptsDynamicLights;
			LandscapeComponent->LightingChannels		= LightingChannels;
			LandscapeComponent->PhysMaterialOverride	= TerrainPhysMaterialOverride;

			// Set the collision display options
			LandscapeComponent->bDisplayCollisionLevel = bShowingCollision;
#endif
		}
	}

#define MAX_HEIGHTMAP_TEXTURE_SIZE 512

	INT ComponentSizeVerts = NumSubsections * (SubsectionSizeQuads+1);
	INT ComponentsPerHeightmap = MAX_HEIGHTMAP_TEXTURE_SIZE / ComponentSizeVerts;

	// Count how many heightmaps we need and the X dimension of the final heightmap
	INT NumHeightmapsX = 1;
	INT FinalComponentsX = NumSectionsX;
	while( FinalComponentsX > ComponentsPerHeightmap )
	{
		FinalComponentsX -= ComponentsPerHeightmap;
		NumHeightmapsX++;
	}
	// Count how many heightmaps we need and the Y dimension of the final heightmap
	INT NumHeightmapsY = 1;
	INT FinalComponentsY = NumSectionsY;
	while( FinalComponentsY > ComponentsPerHeightmap )
	{
		FinalComponentsY -= ComponentsPerHeightmap;
		NumHeightmapsY++;
	}

	TArray<FHeightmapInfo> HeightmapInfos;

	for( INT HmY=0;HmY<NumHeightmapsY;HmY++ )
	{
		for( INT HmX=0;HmX<NumHeightmapsX;HmX++ )
		{
			FHeightmapInfo& HeightmapInfo = HeightmapInfos(HeightmapInfos.AddZeroed());

			// make sure the heightmap UVs are powers of two.
			HeightmapInfo.HeightmapSizeU = (1<<appCeilLogTwo( ((HmX==NumHeightmapsX-1) ? FinalComponentsX : ComponentsPerHeightmap)*ComponentSizeVerts ));
			HeightmapInfo.HeightmapSizeV = (1<<appCeilLogTwo( ((HmY==NumHeightmapsY-1) ? FinalComponentsY : ComponentsPerHeightmap)*ComponentSizeVerts ));

			// Construct the heightmap textures
			HeightmapInfo.HeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GetOutermost(), NAME_None/*FName(TEXT("Heightmap"))*/, RF_Public|RF_Standalone);
			HeightmapInfo.HeightmapTexture->Init(HeightmapInfo.HeightmapSizeU,HeightmapInfo.HeightmapSizeV,PF_A8R8G8B8);
			HeightmapInfo.HeightmapTexture->SRGB = FALSE;
			HeightmapInfo.HeightmapTexture->CompressionNone = TRUE;
			HeightmapInfo.HeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
			HeightmapInfo.HeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Heightmap;
			HeightmapInfo.HeightmapTexture->AddressX = TA_Clamp;
			HeightmapInfo.HeightmapTexture->AddressY = TA_Clamp;

			INT MipSubsectionSizeQuads = SubsectionSizeQuads;
			INT MipSizeU = HeightmapInfo.HeightmapSizeU;
			INT MipSizeV = HeightmapInfo.HeightmapSizeV;
			while( MipSizeU > 1 && MipSizeV > 1 && MipSubsectionSizeQuads >= 1 )
			{
				FColor* HeightmapTextureData;
				if( HeightmapInfo.HeightmapTextureMipData.Num() > 0 )	
				{
					// create subsequent mips
					FTexture2DMipMap* HeightMipMap = new(HeightmapInfo.HeightmapTexture->Mips) FTexture2DMipMap;
					HeightMipMap->SizeX = MipSizeU;
					HeightMipMap->SizeY = MipSizeV;
					HeightMipMap->Data.Lock(LOCK_READ_WRITE);
					HeightmapTextureData = (FColor*)HeightMipMap->Data.Realloc(MipSizeU*MipSizeV*sizeof(FColor));
				}
				else
				{
					HeightmapTextureData = (FColor*)HeightmapInfo.HeightmapTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
				}

				appMemzero( HeightmapTextureData, MipSizeU*MipSizeV*sizeof(FColor) );
				HeightmapInfo.HeightmapTextureMipData.AddItem(HeightmapTextureData);

				MipSizeU >>= 1;
				MipSizeV >>= 1;

				MipSubsectionSizeQuads = ((MipSubsectionSizeQuads + 1) >> 1) - 1;
			}
		}
	}

	// Calculate the normals for each of the two triangles per quad.
	FVector* VertexNormals = new FVector[(NumPatchesX+1)*(NumPatchesY+1)];
	appMemzero(VertexNormals, (NumPatchesX+1)*(NumPatchesY+1)*sizeof(FVector));
	for( INT QuadY=0;QuadY<NumPatchesY;QuadY++ )
	{
		for( INT QuadX=0;QuadX<NumPatchesX;QuadX++ )
		{
			FVector Vert00 = FVector(0.f,0.f,((FLOAT)HEIGHTDATA(QuadX+0, QuadY+0) - 32768.f)/128.f) * DrawScale3D;
			FVector Vert01 = FVector(0.f,1.f,((FLOAT)HEIGHTDATA(QuadX+0, QuadY+1) - 32768.f)/128.f) * DrawScale3D;
			FVector Vert10 = FVector(1.f,0.f,((FLOAT)HEIGHTDATA(QuadX+1, QuadY+0) - 32768.f)/128.f) * DrawScale3D;
			FVector Vert11 = FVector(1.f,1.f,((FLOAT)HEIGHTDATA(QuadX+1, QuadY+1) - 32768.f)/128.f) * DrawScale3D;

			FVector FaceNormal1 = ((Vert00-Vert10) ^ (Vert10-Vert11)).SafeNormal();
			FVector FaceNormal2 = ((Vert11-Vert01) ^ (Vert01-Vert00)).SafeNormal(); 

			// contribute to the vertex normals.
			VertexNormals[(QuadX+1 + (NumPatchesX+1)*(QuadY+0))] += FaceNormal1;
			VertexNormals[(QuadX+0 + (NumPatchesX+1)*(QuadY+1))] += FaceNormal2;
			VertexNormals[(QuadX+0 + (NumPatchesX+1)*(QuadY+0))] += FaceNormal1 + FaceNormal2;
			VertexNormals[(QuadX+1 + (NumPatchesX+1)*(QuadY+1))] += FaceNormal1 + FaceNormal2;
		}
	}

#define LANDSCAPE_WEIGHT_NORMALIZE_THRESHOLD	3

	// Weight values for each layer for each component.
	TArray<TArray<TArray<BYTE> > > ComponentWeightValues;
	ComponentWeightValues.AddZeroed(NumSectionsX*NumSectionsY);

	for (INT ComponentY = 0; ComponentY < NumSectionsY; ComponentY++)
	{
		for (INT ComponentX = 0; ComponentX < NumSectionsX; ComponentX++)
		{
			ULandscapeComponent* LandscapeComponent = LandscapeComponents(ComponentX + ComponentY*NumSectionsX);
			TArray<TArray<BYTE> >& WeightValues = ComponentWeightValues(ComponentX + ComponentY*NumSectionsX);

			//// Assign the components' neighbors 
			//LandscapeComponent->Neighbors[TCN_NW] = ComponentX==0 || ComponentY==0								? NULL : LandscapeComponents((ComponentX-1) + (ComponentY-1)*NumSectionsX);
			//LandscapeComponent->Neighbors[TCN_N]  = ComponentY==0												? NULL : LandscapeComponents((ComponentX  ) + (ComponentY-1)*NumSectionsX);
			//LandscapeComponent->Neighbors[TCN_NE] = ComponentX==NumSectionsX-1 || ComponentY==0					? NULL : LandscapeComponents((ComponentX+1) + (ComponentY-1)*NumSectionsX);
			//LandscapeComponent->Neighbors[TCN_W]  = ComponentX==0												? NULL : LandscapeComponents((ComponentX-1) + (ComponentY  )*NumSectionsX);
			//LandscapeComponent->Neighbors[TCN_E]  = ComponentX==NumSectionsX-1									? NULL : LandscapeComponents((ComponentX+1) + (ComponentY  )*NumSectionsX);
			//LandscapeComponent->Neighbors[TCN_SW] = ComponentX==0 || ComponentY==NumSectionsY-1					? NULL : LandscapeComponents((ComponentX-1) + (ComponentY+1)*NumSectionsX);
			//LandscapeComponent->Neighbors[TCN_S]  = ComponentY==NumSectionsY-1									? NULL : LandscapeComponents((ComponentX  ) + (ComponentY+1)*NumSectionsX);
			//LandscapeComponent->Neighbors[TCN_SE] = ComponentX==NumSectionsX-1 || ComponentY==NumSectionsY-1	? NULL : LandscapeComponents((ComponentX+1) + (ComponentY+1)*NumSectionsX);

			// Import alphamap data into local array and check for unused layers for this component.
			TArray<FLandscapeComponentAlphaInfo> EditingAlphaLayerData;
			for( INT LayerIndex=0;LayerIndex<ImportLayerInfos.Num();LayerIndex++ )
			{
				FLandscapeComponentAlphaInfo* NewAlphaInfo = new(EditingAlphaLayerData) FLandscapeComponentAlphaInfo(LandscapeComponent, LayerIndex);

				for( INT AlphaY=0;AlphaY<=LandscapeComponent->ComponentSizeQuads;AlphaY++ )
				{
					BYTE* OldAlphaRowStart = &AlphaDataPointers[LayerIndex][ (AlphaY+LandscapeComponent->SectionBaseY) * VertsX + (LandscapeComponent->SectionBaseX) ];
					BYTE* NewAlphaRowStart = &NewAlphaInfo->AlphaValues(AlphaY * (LandscapeComponent->ComponentSizeQuads+1));
					appMemcpy(NewAlphaRowStart, OldAlphaRowStart, LandscapeComponent->ComponentSizeQuads+1);
				}						
			}

			for( INT AlphaMapIndex=0; AlphaMapIndex<EditingAlphaLayerData.Num();AlphaMapIndex++ )
			{
				if( EditingAlphaLayerData(AlphaMapIndex).IsLayerAllZero() )
				{
					EditingAlphaLayerData.Remove(AlphaMapIndex);
					AlphaMapIndex--;
				}
			}

			
			debugf(TEXT("%s needs %d alphamaps"), *LandscapeComponent->GetName(),EditingAlphaLayerData.Num());

			// Calculate weightmap weights for this component
			WeightValues.Empty(EditingAlphaLayerData.Num());
			WeightValues.AddZeroed(EditingAlphaLayerData.Num());
			LandscapeComponent->WeightmapLayerAllocations.Empty(EditingAlphaLayerData.Num());

			for( INT WeightLayerIndex=0; WeightLayerIndex<WeightValues.Num();WeightLayerIndex++ )
			{
				// Lookup the original layer name
				WeightValues(WeightLayerIndex) = EditingAlphaLayerData(WeightLayerIndex).AlphaValues;
				new(LandscapeComponent->WeightmapLayerAllocations) FWeightmapLayerAllocationInfo(ImportLayerInfos(EditingAlphaLayerData(WeightLayerIndex).LayerIndex).LayerName);
			}

			// Discard the temporary alpha data
			EditingAlphaLayerData.Empty();

			// For each layer...
			for( INT WeightLayerIndex=WeightValues.Num()-1; WeightLayerIndex>=0;WeightLayerIndex-- )
			{
				// ... multiply all lower layers'...
				for( INT BelowWeightLayerIndex=WeightLayerIndex-1; BelowWeightLayerIndex>=0;BelowWeightLayerIndex-- )
				{
					INT TotalWeight = 0;
					// ... values by...
					for( INT Idx=0;Idx<WeightValues(WeightLayerIndex).Num();Idx++ )
					{
						// ... one-minus the current layer's values
						INT NewValue = (INT)WeightValues(BelowWeightLayerIndex)(Idx) * (INT)(255 - WeightValues(WeightLayerIndex)(Idx)) / 255;
						WeightValues(BelowWeightLayerIndex)(Idx) = (BYTE)NewValue;
						TotalWeight += NewValue;
					}

					if( TotalWeight == 0 )
					{
						// Remove the layer as it has no contribution
						WeightValues.Remove(BelowWeightLayerIndex);
						LandscapeComponent->WeightmapLayerAllocations.Remove(BelowWeightLayerIndex);

						// The current layer has been re-numbered
						WeightLayerIndex--;
					}
				}
			}

			// Weight normalization for total should be 255...
			if (WeightValues.Num())
			{
				for( INT Idx=0;Idx<WeightValues(0).Num();Idx++ )
				{
					INT TotalWeight = 0;
					for( INT WeightLayerIndex = 0; WeightLayerIndex < WeightValues.Num(); WeightLayerIndex++ )
					{
						TotalWeight += (INT)WeightValues(WeightLayerIndex)(Idx);
					}
					if (TotalWeight == 0)
					{
						WeightValues(0)(Idx) = 255;
					}
					else if (255 - TotalWeight <= LANDSCAPE_WEIGHT_NORMALIZE_THRESHOLD) // small difference, currenlty 3...
					{
						WeightValues(0)(Idx) += 255 - TotalWeight;
					}
					else if (TotalWeight != 255)
					{
						// normalization...
						FLOAT Factor = 255.f/TotalWeight;
						TotalWeight = 0;
						for( INT WeightLayerIndex = 0; WeightLayerIndex < WeightValues.Num(); WeightLayerIndex++ )
						{
							WeightValues(WeightLayerIndex)(Idx) = (BYTE)(Factor * WeightValues(WeightLayerIndex)(Idx));
							TotalWeight += WeightValues(WeightLayerIndex)(Idx);
						}
						if (255 - TotalWeight)
						{
							WeightValues(0)(Idx) += 255 - TotalWeight;
						}
					}
				}
			}
		}
	}

	// Remember where we have spare texture channels.
	TArray<FWeightmapTextureAllocation> TextureAllocations;
		
	for (INT ComponentY = 0; ComponentY < NumSectionsY; ComponentY++)
	{
		for (INT ComponentX = 0; ComponentX < NumSectionsX; ComponentX++)
		{
			INT HmX = ComponentX / ComponentsPerHeightmap;
			INT HmY = ComponentY / ComponentsPerHeightmap;
			FHeightmapInfo& HeightmapInfo = HeightmapInfos(HmX + HmY * NumHeightmapsX);

			ULandscapeComponent* LandscapeComponent = LandscapeComponents(ComponentX + ComponentY*NumSectionsX);

			// Lookup array of weight values for this component.
			TArray<TArray<BYTE> >& WeightValues = ComponentWeightValues(ComponentX + ComponentY*NumSectionsX);

			// Heightmap offsets
			INT HeightmapOffsetX = (ComponentX - ComponentsPerHeightmap*HmX) * NumSubsections * (SubsectionSizeQuads+1);
			INT HeightmapOffsetY = (ComponentY - ComponentsPerHeightmap*HmY) * NumSubsections * (SubsectionSizeQuads+1);

			LandscapeComponent->HeightmapScaleBias = FVector4( 1.f / (FLOAT)HeightmapInfo.HeightmapSizeU, 1.f / (FLOAT)HeightmapInfo.HeightmapSizeV, (FLOAT)((HeightmapOffsetX)) / (FLOAT)HeightmapInfo.HeightmapSizeU, ((FLOAT)(HeightmapOffsetY)) / (FLOAT)HeightmapInfo.HeightmapSizeV );
			LandscapeComponent->HeightmapSubsectionOffset =  (FLOAT)(SubsectionSizeQuads+1) / (FLOAT)HeightmapInfo.HeightmapSizeU;

			LandscapeComponent->HeightmapTexture = HeightmapInfo.HeightmapTexture;

			// Layer UV panning to ensure consistent tiling across components
			LandscapeComponent->LayerUVPan = FVector2D( (FLOAT)(ComponentX*ComponentSizeQuads), (FLOAT)(ComponentY*ComponentSizeQuads) );

			// Weightmap is sized the same as the component
			INT WeightmapSize = (SubsectionSizeQuads+1) * NumSubsections;
			// Should be power of two
			check(((WeightmapSize-1) & WeightmapSize) == 0);

			LandscapeComponent->WeightmapScaleBias = FVector4( 1.f / (FLOAT)WeightmapSize, 1.f / (FLOAT)WeightmapSize, 0.5f / (FLOAT)WeightmapSize, 0.5f / (FLOAT)WeightmapSize);
			LandscapeComponent->WeightmapSubsectionOffset =  (FLOAT)(SubsectionSizeQuads+1) / (FLOAT)WeightmapSize;

			// Pointers to the texture data where we'll store each layer. Stride is 4 (FColor)
			TArray<BYTE*> WeightmapTextureDataPointers;

			debugf(TEXT("%s needs %d weightmap channels"), *LandscapeComponent->GetName(),WeightValues.Num());

			// Find texture channels to store each layer.
			INT LayerIndex = 0;
			while( LayerIndex < WeightValues.Num() )
			{
				INT RemainingLayers = WeightValues.Num()-LayerIndex;

				INT BestAllocationIndex = -1;

				// if we need less than 4 channels, try to find them somewhere to put all of them
				if( RemainingLayers < 4 )
				{
					INT BestDistSquared = MAXINT;
					for( INT TryAllocIdx=0;TryAllocIdx<TextureAllocations.Num();TryAllocIdx++ )
					{
						if( TextureAllocations(TryAllocIdx).ChannelsInUse + RemainingLayers <= 4 )
						{
							FWeightmapTextureAllocation& TryAllocation = TextureAllocations(TryAllocIdx);
							INT TryDistSquared = Square(TryAllocation.X-ComponentX) + Square(TryAllocation.Y-ComponentY);
							if( TryDistSquared < BestDistSquared )
							{
								BestDistSquared = TryDistSquared;
								BestAllocationIndex = TryAllocIdx;
							}
						}
					}
				}

				if( BestAllocationIndex != -1 )
				{
					FWeightmapTextureAllocation& Allocation = TextureAllocations(BestAllocationIndex);
					
					debugf(TEXT("  ==> Storing %d channels starting at %s[%d]"), RemainingLayers, *Allocation.Texture->GetName(), Allocation.ChannelsInUse );

					for( INT i=0;i<RemainingLayers;i++ )
					{
						LandscapeComponent->WeightmapLayerAllocations(LayerIndex+i).WeightmapTextureIndex = LandscapeComponent->WeightmapTextures.Num();
						LandscapeComponent->WeightmapLayerAllocations(LayerIndex+i).WeightmapTextureChannel = Allocation.ChannelsInUse;
						switch( Allocation.ChannelsInUse )
						{
						case 1:
							WeightmapTextureDataPointers.AddItem((BYTE*)&Allocation.TextureData->G);
							break;
						case 2:
							WeightmapTextureDataPointers.AddItem((BYTE*)&Allocation.TextureData->B);
							break;
						case 3:
							WeightmapTextureDataPointers.AddItem((BYTE*)&Allocation.TextureData->A);
							break;
						default:
							// this should not occur.
							check(0);

						}
						Allocation.ChannelsInUse++;
					}

					LayerIndex += RemainingLayers;
					LandscapeComponent->WeightmapTextures.AddItem(Allocation.Texture);
				}
				else
				{
					// We couldn't find a suitable place for these layers, so lets make a new one.
					UTexture2D* WeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GetOutermost(), NAME_None, RF_Public|RF_Standalone);
					WeightmapTexture->Init(WeightmapSize,WeightmapSize,PF_A8R8G8B8);
					WeightmapTexture->SRGB = FALSE;
					WeightmapTexture->CompressionNone = TRUE;
					WeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
					WeightmapTexture->AddressX = TA_Clamp;
					WeightmapTexture->AddressY = TA_Clamp;
					WeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Weightmap;
					FColor* MipData = (FColor*)WeightmapTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);

					INT ThisAllocationLayers = Min<INT>(RemainingLayers,4);
					new(TextureAllocations) FWeightmapTextureAllocation(ComponentX,ComponentY,ThisAllocationLayers,WeightmapTexture,MipData);

					debugf(TEXT("  ==> Storing %d channels in new texture %s"), ThisAllocationLayers, *WeightmapTexture->GetName());

					WeightmapTextureDataPointers.AddItem((BYTE*)&MipData->R);
					LandscapeComponent->WeightmapLayerAllocations(LayerIndex+0).WeightmapTextureIndex = LandscapeComponent->WeightmapTextures.Num();
					LandscapeComponent->WeightmapLayerAllocations(LayerIndex+0).WeightmapTextureChannel = 0;

					if( ThisAllocationLayers > 1 )
					{
						WeightmapTextureDataPointers.AddItem((BYTE*)&MipData->G);
						LandscapeComponent->WeightmapLayerAllocations(LayerIndex+1).WeightmapTextureIndex = LandscapeComponent->WeightmapTextures.Num();
						LandscapeComponent->WeightmapLayerAllocations(LayerIndex+1).WeightmapTextureChannel = 1;

						if( ThisAllocationLayers > 2 )
						{
							WeightmapTextureDataPointers.AddItem((BYTE*)&MipData->B);
							LandscapeComponent->WeightmapLayerAllocations(LayerIndex+2).WeightmapTextureIndex = LandscapeComponent->WeightmapTextures.Num();
							LandscapeComponent->WeightmapLayerAllocations(LayerIndex+2).WeightmapTextureChannel = 2;

							if( ThisAllocationLayers > 3 )
							{
								WeightmapTextureDataPointers.AddItem((BYTE*)&MipData->A);
								LandscapeComponent->WeightmapLayerAllocations(LayerIndex+3).WeightmapTextureIndex = LandscapeComponent->WeightmapTextures.Num();
								LandscapeComponent->WeightmapLayerAllocations(LayerIndex+3).WeightmapTextureChannel = 3;
							}
						}
					}
					LandscapeComponent->WeightmapTextures.AddItem(WeightmapTexture);

					LayerIndex += ThisAllocationLayers;
				}
			}
			check(WeightmapTextureDataPointers.Num() == WeightValues.Num());

			FVector* WorldVerts = new FVector[Square(ComponentSizeQuads+1)];

			for( INT SubsectionY = 0;SubsectionY < NumSubsections;SubsectionY++ )
			{
				for( INT SubsectionX = 0;SubsectionX < NumSubsections;SubsectionX++ )
				{
					for( INT SubY=0;SubY<=SubsectionSizeQuads;SubY++ )
					{
						for( INT SubX=0;SubX<=SubsectionSizeQuads;SubX++ )
						{
							// X/Y of the vertex we're looking at in component's coordinates.
							INT CompX = SubsectionSizeQuads * SubsectionX + SubX;
							INT CompY = SubsectionSizeQuads * SubsectionY + SubY;

							// X/Y of the vertex we're looking indexed into the texture data
							INT TexX = (SubsectionSizeQuads+1) * SubsectionX + SubX;
							INT TexY = (SubsectionSizeQuads+1) * SubsectionY + SubY;

							INT WeightSrcDataIdx = CompY * (ComponentSizeQuads+1) + CompX;
							INT HeightTexDataIdx = (HeightmapOffsetX + TexX) + (HeightmapOffsetY + TexY) * (HeightmapInfo.HeightmapSizeU);

							INT WeightTexDataIdx = (TexX) + (TexY) * (WeightmapSize);

							// copy height and normal data
							WORD HeightValue = HEIGHTDATA(CompX + LandscapeComponent->SectionBaseX, CompY + LandscapeComponent->SectionBaseY);
							FVector Normal = VertexNormals[CompX+LandscapeComponent->SectionBaseX + (NumPatchesX+1)*(CompY+LandscapeComponent->SectionBaseY)].SafeNormal();

							HeightmapInfo.HeightmapTextureMipData(0)[HeightTexDataIdx].R = HeightValue >> 8;
							HeightmapInfo.HeightmapTextureMipData(0)[HeightTexDataIdx].G = HeightValue & 255;
							HeightmapInfo.HeightmapTextureMipData(0)[HeightTexDataIdx].B = appRound( 127.5f * (Normal.X + 1.f) );
							HeightmapInfo.HeightmapTextureMipData(0)[HeightTexDataIdx].A = appRound( 127.5f * (Normal.Y + 1.f) );

							for( INT WeightmapIndex=0;WeightmapIndex<WeightValues.Num(); WeightmapIndex++ )
							{
								WeightmapTextureDataPointers(WeightmapIndex)[WeightTexDataIdx*4] = WeightValues(WeightmapIndex)(WeightSrcDataIdx);
							}


							// Get world space verts
							FVector WorldVertex( (FLOAT)LandscapeComponent->SectionBaseX + FLOAT(CompX), (FLOAT)LandscapeComponent->SectionBaseY + FLOAT(CompY), ((FLOAT)HeightValue - 32768.f)/128.f );
							WorldVertex *= DrawScale3D * DrawScale;
							WorldVertex += Location;
							WorldVerts[(LandscapeComponent->ComponentSizeQuads+1) * CompY + CompX] = WorldVertex;
						}
					}
				}
			}

			// This could give us a tighter sphere bounds than just adding the points one by one.
			LandscapeComponent->CachedBoxSphereBounds = FBoxSphereBounds(WorldVerts, Square(ComponentSizeQuads+1));
			delete[] WorldVerts;


			// Update MaterialInstance
			LandscapeComponent->UpdateMaterialInstances();
		}
	}


	// Unlock the weightmaps' base mips
	for( INT AllocationIndex=0;AllocationIndex<TextureAllocations.Num();AllocationIndex++ )
	{
		UTexture2D* WeightmapTexture = TextureAllocations(AllocationIndex).Texture;
		FColor* BaseMipData = TextureAllocations(AllocationIndex).TextureData;

		// Generate mips for weightmaps
		ULandscapeComponent::GenerateWeightmapMips(NumSubsections, SubsectionSizeQuads, WeightmapTexture, BaseMipData);

		WeightmapTexture->Mips(0).Data.Unlock();
		WeightmapTexture->UpdateResource();
	}


	delete[] VertexNormals;

	// Generate mipmaps for the components, and create the collision components
	for (INT ComponentY = 0; ComponentY < NumSectionsY; ComponentY++)
	{
		for (INT ComponentX = 0; ComponentX < NumSectionsX; ComponentX++)
		{
			INT HmX = ComponentX / ComponentsPerHeightmap;
			INT HmY = ComponentY / ComponentsPerHeightmap;
			FHeightmapInfo& HeightmapInfo = HeightmapInfos(HmX + HmY * NumHeightmapsX);

			ULandscapeComponent* LandscapeComponent = LandscapeComponents(ComponentX + ComponentY*NumSectionsX);
			LandscapeComponent->GenerateHeightmapMips(HeightmapInfo.HeightmapTextureMipData);
			LandscapeComponent->UpdateCollisionComponent(HeightmapInfo.HeightmapTextureMipData(LandscapeComponent->CollisionMipLevel));
		}
	}

	for( INT HmIdx=0;HmIdx<HeightmapInfos.Num();HmIdx++ )
	{
		FHeightmapInfo& HeightmapInfo = HeightmapInfos(HmIdx);

		// Add remaining mips down to 1x1 to heightmap texture. These do not represent quads and are just a simple averages of the previous mipmaps. 
		// These mips are not used for sampling in the vertex shader but could be sampled in the pixel shader.
		INT Mip = HeightmapInfo.HeightmapTextureMipData.Num();
		INT MipSizeU = (HeightmapInfo.HeightmapTexture->SizeX) >> Mip;
		INT MipSizeV = (HeightmapInfo.HeightmapTexture->SizeY) >> Mip;
		while( MipSizeU > 1 && MipSizeV > 1 )
		{	
			// Create the mipmap
			FTexture2DMipMap* HeightmapMipMap = new(HeightmapInfo.HeightmapTexture->Mips) FTexture2DMipMap;
			HeightmapMipMap->SizeX = MipSizeU;
			HeightmapMipMap->SizeY = MipSizeV;
			HeightmapMipMap->Data.Lock(LOCK_READ_WRITE);
			HeightmapInfo.HeightmapTextureMipData.AddItem( (FColor*)HeightmapMipMap->Data.Realloc(MipSizeU*MipSizeV*sizeof(FColor)) );

			INT PrevMipSizeU = (HeightmapInfo.HeightmapTexture->SizeX) >> (Mip-1);
			INT PrevMipSizeV = (HeightmapInfo.HeightmapTexture->SizeY) >> (Mip-1);

			for( INT Y = 0;Y < MipSizeV;Y++ )
			{
				for( INT X = 0;X < MipSizeU;X++ )
				{
					FColor* TexData = &(HeightmapInfo.HeightmapTextureMipData(Mip))[ X + Y * MipSizeU ];

					FColor *PreMipTexData00 = &(HeightmapInfo.HeightmapTextureMipData(Mip-1))[ (X*2+0) + (Y*2+0)  * PrevMipSizeU ];
					FColor *PreMipTexData01 = &(HeightmapInfo.HeightmapTextureMipData(Mip-1))[ (X*2+0) + (Y*2+1)  * PrevMipSizeU ];
					FColor *PreMipTexData10 = &(HeightmapInfo.HeightmapTextureMipData(Mip-1))[ (X*2+1) + (Y*2+0)  * PrevMipSizeU ];
					FColor *PreMipTexData11 = &(HeightmapInfo.HeightmapTextureMipData(Mip-1))[ (X*2+1) + (Y*2+1)  * PrevMipSizeU ];

					TexData->R = (((INT)PreMipTexData00->R + (INT)PreMipTexData01->R + (INT)PreMipTexData10->R + (INT)PreMipTexData11->R) >> 2);
					TexData->G = (((INT)PreMipTexData00->G + (INT)PreMipTexData01->G + (INT)PreMipTexData10->G + (INT)PreMipTexData11->G) >> 2);
					TexData->B = (((INT)PreMipTexData00->B + (INT)PreMipTexData01->B + (INT)PreMipTexData10->B + (INT)PreMipTexData11->B) >> 2);
					TexData->A = (((INT)PreMipTexData00->A + (INT)PreMipTexData01->A + (INT)PreMipTexData10->A + (INT)PreMipTexData11->A) >> 2);
				}
			}

			Mip++;
			MipSizeU >>= 1;
			MipSizeV >>= 1;
		}

		for( INT i=0;i<HeightmapInfo.HeightmapTextureMipData.Num();i++ )
		{
			HeightmapInfo.HeightmapTexture->Mips(i).Data.Unlock();
		}
		HeightmapInfo.HeightmapTexture->UpdateResource();
	}

	// Update our new components
	ConditionalUpdateComponents();

	// Init RB physics for editor collision
	InitRBPhysEditor();

	// Add Collision data update
	UpdateAllAddCollisions();

	GWarn->EndSlowTask();
}

UBOOL ALandscape::GetLandscapeExtent(INT& MinX, INT& MinY, INT& MaxX, INT& MaxY)
{
	MinX=MAXINT;
	MinY=MAXINT;
	MaxX=MININT;
	MaxY=MININT;

	// Find range of entire landscape
	for(INT ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++ )
	{
		ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
		if( Comp )
		{
			if( Comp->SectionBaseX < MinX )
			{
				MinX = Comp->SectionBaseX;
			}
			if( Comp->SectionBaseY < MinY )
			{
				MinY = Comp->SectionBaseY;
			}
			if( Comp->SectionBaseX+ComponentSizeQuads > MaxX )
			{
				MaxX = Comp->SectionBaseX+ComponentSizeQuads;
			}
			if( Comp->SectionBaseY+ComponentSizeQuads > MaxY )
			{
				MaxY = Comp->SectionBaseY+ComponentSizeQuads;
			}
		}
	}
	return (MinX != MAXINT);
}

void ALandscape::Export(TArray<FString>& Filenames)
{
	check( Filenames.Num() > 0 );

	INT MinX=MAXINT;
	INT MinY=MAXINT;
	INT MaxX=-MAXINT;
	INT MaxY=-MAXINT;

	if( !GetLandscapeExtent(MinX,MinY,MaxX,MaxY) )
	{
		return;
	}

	GWarn->BeginSlowTask( TEXT("Exporting Landscape"), TRUE);

	FLandscapeEditDataInterface LandscapeEdit(this);

	TArray<BYTE> HeightData;
	HeightData.AddZeroed((1+MaxX-MinX)*(1+MaxY-MinY)*sizeof(WORD));
	LandscapeEdit.GetHeightDataFast(MinX,MinY,MaxX,MaxY,(WORD*)&HeightData(0),0);
	appSaveArrayToFile(HeightData,*Filenames(0));

	for( INT i=1;i<Filenames.Num();i++ )
	{
		TArray<BYTE> WeightData;
		WeightData.AddZeroed((1+MaxX-MinX)*(1+MaxY-MinY));
		LandscapeEdit.GetWeightDataFast(LayerInfos(i-1).LayerName, MinX,MinY,MaxX,MaxY,&WeightData(0),0);
		appSaveArrayToFile(WeightData,*Filenames(i));
	}

	GWarn->EndSlowTask();
}

void ALandscape::DeleteLayer(FName LayerName)
{
	GWarn->BeginSlowTask( TEXT("Deleting Layer"), TRUE);

	// Remove data from all components
	FLandscapeEditDataInterface LandscapeEdit(this);
	LandscapeEdit.DeleteLayer(LayerName);

	// Remove from array
	for( INT LayerIdx=0;LayerIdx<LayerInfos.Num();LayerIdx++ )
	{
		if( LayerInfos(LayerIdx).LayerName == LayerName )
		{
			LayerInfos.Remove(LayerIdx);
			break;
		}
	}

	GWarn->EndSlowTask();
}

void ALandscape::GetAllEditableComponents(TArray<ULandscapeComponent*>* AllLandscapeComponnents, TArray<ULandscapeHeightfieldCollisionComponent*>* AllCollisionComponnents /*= NULL*/)
{
	if (AllLandscapeComponnents)
	{
		*AllLandscapeComponnents = LandscapeComponents;
		// for all proxies...
		for (TSet<ALandscapeProxy*>::TIterator It(Proxies); It; ++It )
		{
			AllLandscapeComponnents->Append((*It)->LandscapeComponents);
		}
	}
	if (AllCollisionComponnents)
	{
		*AllCollisionComponnents = CollisionComponents;
		// for all proxies...
		for (TSet<ALandscapeProxy*>::TIterator It(Proxies); It; ++It )
		{
			AllCollisionComponnents->Append((*It)->CollisionComponents);
		}
	}
}

void ALandscapeProxy::InitRBPhysEditor()
{
	InitRBPhys();
}

void ALandscapeProxy::PostEditMove(UBOOL bFinished)
{
	Super::PostEditMove(bFinished);
	if( bFinished )
	{
		//PostLoad();
		UpdateLandscapeActor(NULL);
		WeightmapUsageMap.Empty();
		for(INT ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++ )
		{
			ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
			if( Comp )
			{
				Comp->SetupActor();
				Comp->UpdateMaterialInstances();
				if (Comp->EditToolRenderData)
				{
					Comp->EditToolRenderData->UpdateDebugColorMaterial();
				}

				FComponentReattachContext ReattachContext(Comp);
			}
		}

		for(INT ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++ )
		{
			ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents(ComponentIndex);
			if( Comp )
			{
				Comp->RecreateHeightfield();
			}
		}

		ConditionalUpdateComponents();
	}
}

void ALandscape::PostEditMove(UBOOL bFinished)
{
	Super::PostEditMove(bFinished);
	if( bFinished )
	{
		Proxies.Empty();
		// Regenerate Proxies
		if (GWorld)
		{
			for (FActorIterator It; It; ++It)
			{
				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
				if (Proxy && Proxy->bIsProxy && Proxy->IsValidLandscapeActor(this))
				{
					Proxy->UpdateLandscapeActor(this);
					Proxy->WeightmapUsageMap.Empty();

					for(INT ComponentIndex = 0; ComponentIndex < Proxy->LandscapeComponents.Num(); ComponentIndex++ )
					{
						ULandscapeComponent* Comp = Proxy->LandscapeComponents(ComponentIndex);
						if( Comp )
						{
							Comp->SetupActor();
							Comp->UpdateMaterialInstances();
							if (Comp->EditToolRenderData)
							{
								Comp->EditToolRenderData->UpdateDebugColorMaterial();
							}

							FComponentReattachContext ReattachContext(Comp);
						}
					}
				}
			}
		}
	}
}

void ALandscapeProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (bIsProxy)
	{
		if( PropertyName == FName(TEXT("LandscapeActorName")) )
		{
			ALandscape* NewLandscapeActor = NULL;
			ALandscape* OldLandscapeActor = LandscapeActor;
			if (GWorld)
			{
				//LandscapeActor = LoadObject<ALandscape>(NULL, *LandscapeActorName, NULL, LOAD_None, NULL);
				for (FActorIterator It; It; ++It)
				{
					ALandscape* Landscape = Cast<ALandscape>(*It);
					if (Landscape && IsValidLandscapeActor(Landscape) )
					{
						NewLandscapeActor = Landscape;
						break;
					}
				}
			}

			// For update, just don't check NewLandscapeActor != OldLandscapeActor
			if (NewLandscapeActor)
			{
				UpdateLandscapeActor(NewLandscapeActor);
				if (OldLandscapeActor && NewLandscapeActor != OldLandscapeActor)
				{
					for (TSet<ALandscapeProxy*>::TIterator It(OldLandscapeActor->Proxies); It; ++It )
					{
						if ( (*It) == this )
						{
							It.RemoveCurrent();
							break;
						}
					}
				}

				UBOOL ChangedMaterial = FALSE;
				UBOOL NeedsRecalcBoundingBox = FALSE;

				NeedsRecalcBoundingBox = TRUE;
				if (OldLandscapeActor->LandscapeMaterial != NewLandscapeActor->LandscapeMaterial)
				{
					ChangedMaterial = TRUE;
				}

				for(INT ComponentIndex = 0; ComponentIndex < LandscapeComponents.Num(); ComponentIndex++ )
				{
					ULandscapeComponent* Comp = LandscapeComponents(ComponentIndex);
					if( Comp )
					{
						if( NeedsRecalcBoundingBox )
						{
							Comp->UpdateCachedBounds();
							Comp->UpdateBounds();
						}

						if( ChangedMaterial )
						{
							// Update the MIC
							Comp->UpdateMaterialInstances();
						}
						// Reattach all components
						FComponentReattachContext ReattachContext(Comp);
					}
				}

				// Update collision
				if( NeedsRecalcBoundingBox )
				{
					for(INT ComponentIndex = 0; ComponentIndex < CollisionComponents.Num(); ComponentIndex++ )
					{
						ULandscapeHeightfieldCollisionComponent* Comp = CollisionComponents(ComponentIndex);
						if( Comp )
						{
							Comp->RecreateHeightfield();
						}
					}
				}

				ConditionalUpdateComponents();
			}
		}
	}
}

void ALandscape::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	UBOOL ChangedMaterial = FALSE;
	UBOOL NeedsRecalcBoundingBox = FALSE;
	UBOOL NeedChangeLighting = FALSE;
	if( PropertyName == FName(TEXT("LandscapeMaterial")) )
	{
		for (INT LayerIdx = 0; LayerIdx < LayerInfos.Num(); LayerIdx++)
		{
			LayerInfos(LayerIdx).ThumbnailMIC = NULL;
		}
		ChangedMaterial = TRUE;
	
		// Clear the parents out of combination material instances
		for( TMap<FString ,UMaterialInstanceConstant*>::TIterator It(MaterialInstanceConstantMap); It; ++It )
		{
			It.Value()->SetParent(NULL);
		}
		
		// Remove our references to any material instances
		MaterialInstanceConstantMap.Empty();
	}
	else if( PropertyName == FName(TEXT("DrawScale")) )
	{
		NeedsRecalcBoundingBox = TRUE;
	}
	else if( PropertyName == FName(TEXT("X")) )
	{
		NeedsRecalcBoundingBox = TRUE;
	}
	else if( PropertyName == FName(TEXT("Y")) )
	{
		NeedsRecalcBoundingBox = TRUE;
	}
	else if( PropertyName == FName(TEXT("Z")) )
	{
		NeedsRecalcBoundingBox = TRUE;
	}
	else if ( GIsEditor && PropertyName == FName(TEXT("StreamingDistanceMultiplier")) )
	{
		// Recalculate in a few seconds.
		ULevel::TriggerStreamingDataRebuild();
	}
	else if ( GIsEditor && PropertyName == FName(TEXT("Hardness")) )
	{
		for (INT LayerIdx = 0; LayerIdx < LayerInfos.Num(); LayerIdx++)
		{
			LayerInfos(LayerIdx).Hardness = Clamp<FLOAT>(LayerInfos(LayerIdx).Hardness, 0.f, 1.f);
		}
	}
	else if ( GIsEditor && PropertyName == FName(TEXT("StaticLightingResolution")) )
	{
		NeedChangeLighting = TRUE;
	}

	if (NeedsRecalcBoundingBox || NeedChangeLighting)
	{
		// Propagate Event to Proxies...
		for (TSet<ALandscapeProxy*>::TIterator It(Proxies); It; ++It )
		{
			(*It)->GetSharedProperties(this);
			(*It)->PostEditChangeProperty(PropertyChangedEvent);
		}
	}

	TArray<ULandscapeComponent*> AllLandscapeComponents;
	GetAllEditableComponents(&AllLandscapeComponents);
	for(INT ComponentIndex = 0; ComponentIndex < AllLandscapeComponents.Num(); ComponentIndex++ )
	{
		ULandscapeComponent* Comp = AllLandscapeComponents(ComponentIndex);
		if( Comp )
		{
			if( NeedsRecalcBoundingBox )
			{
				Comp->UpdateCachedBounds();
				Comp->UpdateBounds();
			}

			if( ChangedMaterial )
			{
				// Update the MIC
				Comp->UpdateMaterialInstances();
			}

			// Reattach all components
			FComponentReattachContext ReattachContext(Comp);
		}
	}

	// Update collision
	if( NeedsRecalcBoundingBox )
	{
		TArray<ULandscapeHeightfieldCollisionComponent*> AllCollisionComponents;
		GetAllEditableComponents(NULL, &AllCollisionComponents);
		for(INT ComponentIndex = 0; ComponentIndex < AllCollisionComponents.Num(); ComponentIndex++ )
		{
			ULandscapeHeightfieldCollisionComponent* Comp = AllCollisionComponents(ComponentIndex);
			if( Comp )
			{
				Comp->RecreateHeightfield();
			}
		}
	}

	if (ChangedMaterial)
	{
		// Update all the proxies...
		for ( TSet<ALandscapeProxy*>::TIterator It(Proxies); It; ++It )
		{
			(*It)->ConditionalUpdateComponents();
		}
	}
}

void ALandscape::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	UBOOL NeedsRecalcBoundingBox = FALSE;

	if ( PropertyChangedEvent.PropertyChain.Num() > 0 )
	{
		UProperty* OutermostProperty = PropertyChangedEvent.PropertyChain.GetHead()->GetValue();
		if ( OutermostProperty != NULL )
		{
			const FName PropertyName = OutermostProperty->GetFName();
			if( PropertyName == FName(TEXT("DrawScale3D")) )
			{
				NeedsRecalcBoundingBox = TRUE;
			}
		}
	}

	if( NeedsRecalcBoundingBox )
	{
		TArray<ULandscapeComponent*> AllLandscapeComponents;
		GetAllEditableComponents(&AllLandscapeComponents);
		for(INT ComponentIndex = 0; ComponentIndex < AllLandscapeComponents.Num(); ComponentIndex++ )
		{
			ULandscapeComponent* Comp = AllLandscapeComponents(ComponentIndex);
			if( Comp )
			{
				Comp->UpdateCachedBounds();
				Comp->UpdateBounds();

				// Reattach all components
				FComponentReattachContext ReattachContext(Comp);
			}
		}
	}

	// Update collision
	if( NeedsRecalcBoundingBox )
	{
		TArray<ULandscapeHeightfieldCollisionComponent*> AllCollisionComponents;
		GetAllEditableComponents(NULL, &AllCollisionComponents);
		for(INT ComponentIndex = 0; ComponentIndex < AllCollisionComponents.Num(); ComponentIndex++ )
		{
			ULandscapeHeightfieldCollisionComponent* Comp = AllCollisionComponents(ComponentIndex);
			if( Comp )
			{
				Comp->RecreateHeightfield();
			}
		}
	}
}

// For Selection Tool
TSet<ULandscapeComponent*> ALandscape::SelectedComponents;
TSet<ULandscapeHeightfieldCollisionComponent*> ALandscape::SelectedCollisionComponents;

void ALandscape::UpdateSelection(ALandscape* Landscape, TSet<ULandscapeComponent*>& NewComponents)
{
	if (Landscape)
	{
		SelectedCollisionComponents.Empty();
		for( TSet<ULandscapeComponent*>::TIterator It(NewComponents); It; ++It )
		{
			if( (*It)->EditToolRenderData != NULL )
			{
				(*It)->EditToolRenderData->UpdateSelectionMaterial(TRUE);
			}

			// Update SelectedCollisionComponents
			ULandscapeHeightfieldCollisionComponent* CollisionComponent = Landscape->XYtoCollisionComponentMap.FindRef(ALandscape::MakeKey((*It)->SectionBaseX, (*It)->SectionBaseY));
			if (CollisionComponent)
			{
				SelectedCollisionComponents.Add(CollisionComponent);
			}
		}

		// Remove the material from any old components that are no longer in the region
		TSet<ULandscapeComponent*> RemovedComponents = SelectedComponents.Difference(NewComponents);
		for ( TSet<ULandscapeComponent*>::TIterator It(RemovedComponents); It; ++It )
		{
			if( (*It)->EditToolRenderData != NULL )
			{
				(*It)->EditToolRenderData->UpdateSelectionMaterial(FALSE);
			}
		}

		SelectedComponents = NewComponents;	
	}
}

IMPLEMENT_COMPARE_CONSTPOINTER( ULandscapeComponent, Landscape, { return (A->SectionBaseX == B->SectionBaseX) ? (A->SectionBaseY - B->SectionBaseY) : (A->SectionBaseX - B->SectionBaseX); } );

void ALandscape::SortSelection()
{
	SelectedComponents.Sort<COMPARE_CONSTPOINTER_CLASS(ULandscapeComponent, Landscape)>();
}

struct FLandscapeDataInterface* ALandscape::GetDataInterface()
{
	if( DataInterface == NULL )
	{ 
		DataInterface = new FLandscapeDataInterface();
	}

	return DataInterface;
}

void ULandscapeComponent::ReallocateWeightmaps(FLandscapeEditDataInterface* DataInterface)
{
	//ALandscape* Landscape = GetLandscapeActor();
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	
	INT NeededNewChannels=0;
	for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
	{
		if( WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex == 255 )
		{
			NeededNewChannels++;
		}
	}

	// All channels allocated!
	if( NeededNewChannels == 0 )
	{
		return;
	}

	Modify();
	//Landscape->Modify();
	Proxy->Modify();

	// debugf(TEXT("----------------------"));
	// debugf(TEXT("Component %s needs %d layers (%d new)"), *GetName(), WeightmapLayerAllocations.Num(), NeededNewChannels);

	// See if our existing textures have sufficient space
	INT ExistingTexAvailableChannels=0;
	for( INT TexIdx=0;TexIdx<WeightmapTextures.Num();TexIdx++ )
	{
		FLandscapeWeightmapUsage* Usage = Proxy->WeightmapUsageMap.Find(WeightmapTextures(TexIdx));
		check(Usage);

		ExistingTexAvailableChannels += Usage->FreeChannelCount();

		if( ExistingTexAvailableChannels >= NeededNewChannels )
		{
			break;
		}
	}
	
	if( ExistingTexAvailableChannels >= NeededNewChannels )
	{
		// debugf(TEXT("Existing texture has available channels"));

		// Allocate using our existing textures' spare channels.
		for( INT TexIdx=0;TexIdx<WeightmapTextures.Num();TexIdx++ )
		{
			FLandscapeWeightmapUsage* Usage = Proxy->WeightmapUsageMap.Find(WeightmapTextures(TexIdx));
			
			for( INT ChanIdx=0;ChanIdx<4;ChanIdx++ )
			{
				if( Usage->ChannelUsage[ChanIdx]==NULL )
				{
					for( INT LayerIdx=0;LayerIdx < WeightmapLayerAllocations.Num();LayerIdx++ )
					{
						FWeightmapLayerAllocationInfo& AllocInfo = WeightmapLayerAllocations(LayerIdx);
						if( AllocInfo.WeightmapTextureIndex == 255 )
						{
							// Zero out the data for this texture channel
							if( DataInterface )
							{
								DataInterface->ZeroTextureChannel( WeightmapTextures(TexIdx), ChanIdx );
							}

							AllocInfo.WeightmapTextureIndex = TexIdx;
							AllocInfo.WeightmapTextureChannel = ChanIdx;
							Usage->ChannelUsage[ChanIdx] = this;
							NeededNewChannels--;

							if( NeededNewChannels == 0 )
							{
								return;
							}
						}
					}
				}
			}
		}
		// we should never get here.
		check(FALSE);
	}

	// debugf(TEXT("Reallocating."));

	// We are totally reallocating the weightmap
	INT TotalNeededChannels = WeightmapLayerAllocations.Num();
	INT CurrentLayer = 0;
	TArray<UTexture2D*> NewWeightmapTextures;
	while( TotalNeededChannels > 0 )
	{
		// debugf(TEXT("Still need %d channels"), TotalNeededChannels);

		UTexture2D* CurrentWeightmapTexture = NULL;
		FLandscapeWeightmapUsage* CurrentWeightmapUsage = NULL;

		if( TotalNeededChannels < 4 )
		{
			// debugf(TEXT("Looking for nearest"));

			// see if we can find a suitable existing weightmap texture with sufficient channels
			INT BestDistanceSquared = MAXINT;
			for( TMap<UTexture2D*,struct FLandscapeWeightmapUsage>::TIterator It(Proxy->WeightmapUsageMap); It; ++It )
			{
				FLandscapeWeightmapUsage* TryWeightmapUsage = &It.Value();
				if( TryWeightmapUsage->FreeChannelCount() >= TotalNeededChannels )
				{
					// See if this candidate is closer than any others we've found
					for( INT ChanIdx=0;ChanIdx<4;ChanIdx++ )
					{
						if( TryWeightmapUsage->ChannelUsage[ChanIdx] != NULL  )
						{
							INT TryDistanceSquared = Square(TryWeightmapUsage->ChannelUsage[ChanIdx]->SectionBaseX - SectionBaseX) + Square(TryWeightmapUsage->ChannelUsage[ChanIdx]->SectionBaseX - SectionBaseX);
							if( TryDistanceSquared < BestDistanceSquared )
							{
								CurrentWeightmapTexture = It.Key();
								CurrentWeightmapUsage = TryWeightmapUsage;
								BestDistanceSquared = TryDistanceSquared;
							}
						}
					}
				}
			}
		}

		UBOOL NeedsUpdateResource=FALSE;
		// No suitable weightmap texture
		if( CurrentWeightmapTexture == NULL )
		{
			MarkPackageDirty();

			// Weightmap is sized the same as the component
			INT WeightmapSize = (SubsectionSizeQuads+1) * NumSubsections;

			// We need a new weightmap texture
			CurrentWeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GetOutermost(), NAME_None, RF_Public|RF_Standalone);
			CurrentWeightmapTexture->Init(WeightmapSize,WeightmapSize,PF_A8R8G8B8);
			CurrentWeightmapTexture->SRGB = FALSE;
			CurrentWeightmapTexture->CompressionNone = TRUE;
			CurrentWeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
			CurrentWeightmapTexture->AddressX = TA_Clamp;
			CurrentWeightmapTexture->AddressY = TA_Clamp;
			CurrentWeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Weightmap;
			// Alloc dummy mips
			CreateEmptyWeightmapMips(CurrentWeightmapTexture);
			CurrentWeightmapTexture->UpdateResource();

			// Store it in the usage map
			CurrentWeightmapUsage = &Proxy->WeightmapUsageMap.Set(CurrentWeightmapTexture, FLandscapeWeightmapUsage());

			// debugf(TEXT("Making a new texture %s"), *CurrentWeightmapTexture->GetName());
		}

		NewWeightmapTextures.AddItem(CurrentWeightmapTexture);

		for( INT ChanIdx=0;ChanIdx<4 && TotalNeededChannels > 0;ChanIdx++ )
		{
			// debugf(TEXT("Finding allocation for layer %d"), CurrentLayer);

			if( CurrentWeightmapUsage->ChannelUsage[ChanIdx] == NULL  )
			{
				// Use this allocation
				FWeightmapLayerAllocationInfo& AllocInfo = WeightmapLayerAllocations(CurrentLayer);

				if( AllocInfo.WeightmapTextureIndex == 255 )
				{
					// New layer - zero out the data for this texture channel
					if( DataInterface )
					{
						DataInterface->ZeroTextureChannel( CurrentWeightmapTexture, ChanIdx );
						// debugf(TEXT("Zeroing out channel %s.%d"), *CurrentWeightmapTexture->GetName(), ChanIdx);
					}
				}
				else
				{
					UTexture2D* OldWeightmapTexture = WeightmapTextures(AllocInfo.WeightmapTextureIndex);

					// Copy the data
					if( DataInterface )
					{
						DataInterface->CopyTextureChannel( CurrentWeightmapTexture, ChanIdx, OldWeightmapTexture, AllocInfo.WeightmapTextureChannel );
						DataInterface->ZeroTextureChannel( OldWeightmapTexture, AllocInfo.WeightmapTextureChannel );
						// debugf(TEXT("Copying old channel (%s).%d to new channel (%s).%d"), *OldWeightmapTexture->GetName(), AllocInfo.WeightmapTextureChannel, *CurrentWeightmapTexture->GetName(), ChanIdx);
					}

					// Remove the old allocation
					FLandscapeWeightmapUsage* OldWeightmapUsage = Proxy->WeightmapUsageMap.Find(OldWeightmapTexture);
					OldWeightmapUsage->ChannelUsage[AllocInfo.WeightmapTextureChannel] = NULL;
				}

				// Assign the new allocation
				CurrentWeightmapUsage->ChannelUsage[ChanIdx] = this;
				AllocInfo.WeightmapTextureIndex = NewWeightmapTextures.Num()-1;
				AllocInfo.WeightmapTextureChannel = ChanIdx;
				CurrentLayer++;
				TotalNeededChannels--;
			}
		}
	}

	// Replace the weightmap textures
	WeightmapTextures = NewWeightmapTextures;

	if (DataInterface)
	{
		// Update the mipmaps for the textures we edited
		for( INT Idx=0;Idx<WeightmapTextures.Num();Idx++)
		{
			UTexture2D* WeightmapTexture = WeightmapTextures(Idx);
			FLandscapeTextureDataInfo* WeightmapDataInfo = DataInterface->GetTextureDataInfo(WeightmapTexture);

			INT NumMips = WeightmapTexture->Mips.Num();
			TArray<FColor*> WeightmapTextureMipData(NumMips);
			for( INT MipIdx=0;MipIdx<NumMips;MipIdx++ )
			{
				WeightmapTextureMipData(MipIdx) = (FColor*)WeightmapDataInfo->GetMipData(MipIdx);
			}

			ULandscapeComponent::UpdateWeightmapMips(NumSubsections, SubsectionSizeQuads, WeightmapTexture, WeightmapTextureMipData, 0, 0, MAXINT, MAXINT, WeightmapDataInfo);
		}
	}
}

void ALandscapeProxy::RemoveInvalidWeightmaps()
{
	if (GIsEditor)
	{
		for( TMap< UTexture2D*,struct FLandscapeWeightmapUsage >::TIterator It(WeightmapUsageMap); It; ++It )
		{
			UTexture2D* Tex = It.Key();
			FLandscapeWeightmapUsage& Usage = It.Value();
			if (Usage.FreeChannelCount() == 4) // Invalid Weight-map
			{
				Tex->SetFlags(RF_Transactional);
				Tex->Modify();
				Tex->MarkPackageDirty();
				Tex->ClearFlags(RF_Standalone);
			}
		}

		// Remove Unused Weightmaps...
		for (INT Idx=0; Idx < LandscapeComponents.Num(); ++Idx)
		{
			ULandscapeComponent* Component = LandscapeComponents(Idx);
			Component->RemoveInvalidWeightmaps();
		}
	}
}

void ULandscapeComponent::RemoveInvalidWeightmaps()
{
	// Adjust WeightmapTextureIndex index for other layers
	TSet<INT> UsedTextureIndices;
	TSet<INT> AllTextureIndices;
	for( INT LayerIdx=0;LayerIdx<WeightmapLayerAllocations.Num();LayerIdx++ )
	{
		UsedTextureIndices.Add( WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex );
	}

	for ( INT WeightIdx=0; WeightIdx < WeightmapTextures.Num(); ++WeightIdx )
	{
		AllTextureIndices.Add( WeightIdx );
	}

	TSet<INT> UnUsedTextureIndices = AllTextureIndices.Difference(UsedTextureIndices);

	INT DeletedLayers = 0;
	for (TSet<INT>::TIterator It(UnUsedTextureIndices); It; ++It)
	{
		INT DeleteLayerWeightmapTextureIndex = *It - DeletedLayers;
		WeightmapTextures(DeleteLayerWeightmapTextureIndex)->SetFlags(RF_Transactional);
		WeightmapTextures(DeleteLayerWeightmapTextureIndex)->Modify();
		WeightmapTextures(DeleteLayerWeightmapTextureIndex)->MarkPackageDirty();
		WeightmapTextures(DeleteLayerWeightmapTextureIndex)->ClearFlags(RF_Standalone);
		WeightmapTextures.Remove( DeleteLayerWeightmapTextureIndex );

		// Adjust WeightmapTextureIndex index for other layers
		for( INT LayerIdx=0;LayerIdx<WeightmapLayerAllocations.Num();LayerIdx++ )
		{
			FWeightmapLayerAllocationInfo& Allocation = WeightmapLayerAllocations(LayerIdx);

			if( Allocation.WeightmapTextureIndex > DeleteLayerWeightmapTextureIndex )
			{
				Allocation.WeightmapTextureIndex--;
			}

			check( Allocation.WeightmapTextureIndex < WeightmapTextures.Num() );
		}
		DeletedLayers++;
	}
}

void ULandscapeComponent::InitHeightmapData(TArray<FColor>& Heights)
{
	INT ComponentSizeVerts = NumSubsections * (SubsectionSizeQuads+1);

	if (Heights.Num() != Square(ComponentSizeVerts) )
	{
		return;
	}

	// Handling old Height map....
	if (HeightmapTexture && HeightmapTexture->GetOutermost() != UObject::GetTransientPackage() 
		&& HeightmapTexture->GetOutermost() == GetOutermost()
		&& HeightmapTexture->SizeX >= ComponentSizeVerts) // if Height map is not valid...
	{
		HeightmapTexture->SetFlags(RF_Transactional);
		HeightmapTexture->Modify();
		HeightmapTexture->MarkPackageDirty();
		HeightmapTexture->ClearFlags(RF_Standalone); // Delete if no reference...
	}

	// New Height map
	TArray<FColor*> HeightmapTextureMipData;
	// make sure the heightmap UVs are powers of two.
	INT HeightmapSizeU = (1<<appCeilLogTwo( ComponentSizeVerts ));
	INT HeightmapSizeV = (1<<appCeilLogTwo( ComponentSizeVerts ));

	// Height map construction
	HeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GetOutermost(), NAME_None, RF_Public|RF_Standalone);
	HeightmapTexture->Init(HeightmapSizeU,HeightmapSizeV,PF_A8R8G8B8);
	HeightmapTexture->SRGB = FALSE;
	HeightmapTexture->CompressionNone = TRUE;
	HeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
	HeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Heightmap;
	HeightmapTexture->AddressX = TA_Clamp;
	HeightmapTexture->AddressY = TA_Clamp;

	INT MipSubsectionSizeQuads = SubsectionSizeQuads;
	INT MipSizeU = HeightmapSizeU;
	INT MipSizeV = HeightmapSizeV;

	HeightmapScaleBias = FVector4( 1.f / (FLOAT)HeightmapSizeU, 1.f / (FLOAT)HeightmapSizeV, 0.f, 0.f);
	HeightmapSubsectionOffset =  (FLOAT)(SubsectionSizeQuads+1) / (FLOAT)HeightmapSizeU;
	LayerUVPan = FVector2D( 0.f, 0.f );

	INT Mip = 0;
	while( MipSizeU > 1 && MipSizeV > 1 && MipSubsectionSizeQuads >= 1 )
	{
		FColor* HeightmapTextureData;
		if( HeightmapTextureMipData.Num() > 0 )	
		{
			// create subsequent mips
			FTexture2DMipMap* HeightMipMap = new(HeightmapTexture->Mips) FTexture2DMipMap;
			HeightMipMap->SizeX = MipSizeU;
			HeightMipMap->SizeY = MipSizeV;
			HeightMipMap->Data.Lock(LOCK_READ_WRITE);
			HeightmapTextureData = (FColor*)HeightMipMap->Data.Realloc(MipSizeU*MipSizeV*sizeof(FColor));
			appMemzero( HeightmapTextureData, MipSizeU*MipSizeV*sizeof(FColor) );
		}
		else
		{
			HeightmapTextureData = (FColor*)HeightmapTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
			appMemcpy( HeightmapTextureData, &Heights(0), MipSizeU*MipSizeV*sizeof(FColor) );
		}

		HeightmapTextureMipData.AddItem(HeightmapTextureData);

		MipSizeU >>= 1;
		MipSizeV >>= 1;
		Mip++;

		MipSubsectionSizeQuads = ((MipSubsectionSizeQuads + 1) >> 1) - 1;
	}
	ULandscapeComponent::GenerateHeightmapMips( HeightmapTextureMipData );
	ULandscapeComponent::UpdateCollisionComponent( HeightmapTextureMipData(CollisionMipLevel) );

	for( INT i=0;i<HeightmapTextureMipData.Num();i++ )
	{
		HeightmapTexture->Mips(i).Data.Unlock();
	}
	HeightmapTexture->UpdateResource();

	//FlushRenderingCommands();
	// Reattach
	//FComponentReattachContext ReattachContext(this);
}

void ULandscapeComponent::InitWeightmapData(TArray<FName>& LayerNames, TArray<TArray<BYTE> >& WeightmapData)
{
	if (LayerNames.Num() != WeightmapData.Num() || LayerNames.Num() <= 0)
	{
		return;
	}

	INT ComponentSizeVerts = NumSubsections * (SubsectionSizeQuads+1);

	// Validation..
	for (INT Idx = 0; Idx < WeightmapData.Num(); ++Idx)
	{
		if ( WeightmapData(Idx).Num() != Square(ComponentSizeVerts) )
		{
			return;
		}
	}
	
	for (INT Idx = 0; Idx < WeightmapTextures.Num(); ++Idx)
	{
		if (WeightmapTextures(Idx) && WeightmapTextures(Idx)->GetOutermost() != UObject::GetTransientPackage() 
			&& WeightmapTextures(Idx)->GetOutermost() == GetOutermost()
			&& WeightmapTextures(Idx)->SizeX == ComponentSizeVerts) 
		{
			WeightmapTextures(Idx)->SetFlags(RF_Transactional);
			WeightmapTextures(Idx)->Modify();
			WeightmapTextures(Idx)->MarkPackageDirty();
			WeightmapTextures(Idx)->ClearFlags(RF_Standalone); // Delete if no reference...
		}
	}
	WeightmapTextures.Empty();

	WeightmapLayerAllocations.Empty(LayerNames.Num());
	for (INT Idx = 0; Idx < LayerNames.Num(); ++Idx)
	{
		new (WeightmapLayerAllocations) FWeightmapLayerAllocationInfo(LayerNames(Idx));
	}

	ReallocateWeightmaps(NULL);

	check(WeightmapLayerAllocations.Num() > 0 && WeightmapTextures.Num() > 0 );

	INT WeightmapSize = ComponentSizeVerts;
	WeightmapScaleBias = FVector4( 1.f / (FLOAT)WeightmapSize, 1.f / (FLOAT)WeightmapSize, 0.5f / (FLOAT)WeightmapSize, 0.5f / (FLOAT)WeightmapSize);
	WeightmapSubsectionOffset =  (FLOAT)(SubsectionSizeQuads+1) / (FLOAT)WeightmapSize;

	// Channel remapping
	INT ChannelOffsets[4] = {STRUCT_OFFSET(FColor,R),STRUCT_OFFSET(FColor,G),STRUCT_OFFSET(FColor,B),STRUCT_OFFSET(FColor,A)};

	TArray<FLandscapeTextureDataInfo> WeightmapDataInfos;
	for (INT WeightmapIdx = 0; WeightmapIdx < WeightmapTextures.Num(); ++WeightmapIdx)
	{
		new (WeightmapDataInfos) FLandscapeTextureDataInfo(WeightmapTextures(WeightmapIdx));
	}

	for (INT LayerIdx = 0; LayerIdx < WeightmapLayerAllocations.Num(); ++LayerIdx)
	{
		FLandscapeTextureDataInfo* DestDataInfo = &WeightmapDataInfos(WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
		BYTE* DestTextureData = (BYTE*)DestDataInfo->GetMipData(0) + ChannelOffsets[ WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel ];
		BYTE* SrcTextureData = (BYTE*)&WeightmapData(LayerIdx)(0);

		for( INT i=0;i<WeightmapData(LayerIdx).Num();i++ )
		{
			DestTextureData[i*4] = SrcTextureData[i];
		}
	}

	// Update the mipmaps for the textures we edited
	for( INT Idx=0;Idx<WeightmapTextures.Num();Idx++)
	{
		UTexture2D* WeightmapTexture = WeightmapTextures(Idx);
		FLandscapeTextureDataInfo* WeightmapDataInfo = &WeightmapDataInfos(Idx);

		WeightmapDataInfo->AddMipUpdateRegion(0, 0, 0, WeightmapTextures(Idx)->SizeX, WeightmapTextures(Idx)->SizeY);

		INT NumMips = WeightmapTexture->Mips.Num();
		TArray<FColor*> WeightmapTextureMipData(NumMips);
		for( INT MipIdx=0;MipIdx<NumMips;MipIdx++ )
		{
			WeightmapTextureMipData(MipIdx) = (FColor*)WeightmapDataInfo->GetMipData(MipIdx);
		}

		ULandscapeComponent::UpdateWeightmapMips(NumSubsections, SubsectionSizeQuads, WeightmapTexture, WeightmapTextureMipData, 0, 0, MAXINT, MAXINT, WeightmapDataInfo);
	}

	for (INT WeightmapIdx = 0; WeightmapIdx < WeightmapDataInfos.Num(); ++WeightmapIdx)
	{
		WeightmapDataInfos(WeightmapIdx).UpdateTextureData();
	}

	FlushRenderingCommands();

	MaterialInstance = NULL;

	//UpdateMaterialInstances();
}

#define MAX_LANDSCAPE_EXPORT_COMPONENTS_NUM		16
#define MAX_LANDSCAPE_PROP_TEXT_LENGTH			1024*1024*16

// Export/Import
UBOOL ALandscapeProxy::ShouldExport()
{
	if (LandscapeComponents.Num() > MAX_LANDSCAPE_EXPORT_COMPONENTS_NUM)
	{
		// Prompt to save startup packages
		INT PopupResult = appMsgf(AMT_YesNo, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("LandscapeExport_Warning"), LandscapeComponents.Num())));

		switch( PopupResult )
		{
		case ART_No:
			return FALSE;
			break;
		case ART_Yes:
			return TRUE;
			break;
		}
	}
	return TRUE;
}

UBOOL ALandscapeProxy::ShouldImport(FString* ActorPropString)
{
	if (ActorPropString && ActorPropString->Len() > MAX_LANDSCAPE_PROP_TEXT_LENGTH)
	{
		// Prompt to save startup packages
		INT PopupResult = appMsgf(AMT_YesNo, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("LandscapeImport_Warning"), ActorPropString->Len() >> 20 )));

		switch( PopupResult )
		{
		case ART_No:
			return FALSE;
			break;
		case ART_Yes:
			return TRUE;
			break;
		}
	}
	return TRUE;
}

void ULandscapeComponent::ExportCustomProperties(FOutputDevice& Out, UINT Indent)
{
	// Height map
	INT NumVertices = Square( NumSubsections*(SubsectionSizeQuads+1) );
	FLandscapeComponentDataInterface DataInterface(this);
	TArray<FColor> Heightmap;
	DataInterface.GetHeightmapTextureData(Heightmap);
	check(Heightmap.Num() == NumVertices);

	Out.Logf( TEXT("%sCustomProperties LandscapeHeightData "), appSpc(Indent));
	for( INT i=0;i<NumVertices;i++ )
	{
		Out.Logf( TEXT("%u "), Heightmap(i).DWColor() );
	}

	TArray<BYTE> Weightmap;
	// Weight map
	Out.Logf( TEXT("LayerNum=%d "), WeightmapLayerAllocations.Num());
	for (INT i=0; i < WeightmapLayerAllocations.Num(); i++)
	{
		if (DataInterface.GetWeightmapTextureData(WeightmapLayerAllocations(i).LayerName, Weightmap))
		{
			Out.Logf( TEXT("LayerName=%s "), *WeightmapLayerAllocations(i).LayerName.ToString());
			for( INT i=0;i<NumVertices;i++ )
			{
				Out.Logf( TEXT("%d "), Weightmap(i) );
			}
		}
	}

	Out.Logf( TEXT("\r\n") );
}

void ULandscapeComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	if(ParseCommand(&SourceText,TEXT("LandscapeHeightData")))
	{
		INT NumVertices = Square( NumSubsections*(SubsectionSizeQuads+1) );

		TArray<FColor> Heights;
		Heights.Empty(NumVertices);
		Heights.AddZeroed(NumVertices);

		ParseNext(&SourceText);
		INT i = 0;
		TCHAR* StopStr;
		while( appIsDigit(*SourceText) ) 
		{
			if( i < NumVertices )
			{
				Heights(i++).DWColor() = appStrtoi(SourceText, &StopStr, 10);
				while( appIsDigit(*SourceText) ) 
				{
					SourceText++;
				}
			}

			ParseNext(&SourceText);
		} 

		if( i != NumVertices )
		{
			Warn->Logf( *LocalizeError(TEXT("CustomProperties(LanscapeComponent Heightmap) Syntax Error"), TEXT("Core")));
		}

		INT ComponentSizeVerts = NumSubsections * (SubsectionSizeQuads+1);

		InitHeightmapData(Heights);

		// Weight maps
		INT LayerNum = 0;
		if (Parse(SourceText, TEXT("LayerNum="), LayerNum))
		{
			while(*SourceText && (!appIsWhitespace(*SourceText)))
			{
				++SourceText;
			}
			ParseNext(&SourceText);
		}

		if (LayerNum <= 0)
		{
			return;
		}

		// Init memory
		TArray<FName> LayerNames;
		LayerNames.Empty(LayerNum);
		TArray<TArray<BYTE>> WeightmapData;
		for (INT i=0; i < LayerNum; ++i)
		{
			TArray<BYTE> Weights;
			Weights.Empty(NumVertices);
			Weights.Add(NumVertices);
			WeightmapData.AddItem(Weights);
		}

		INT LayerIdx = 0;
		FString LayerName;
		while ( *SourceText )
		{
			if (Parse(SourceText, TEXT("LayerName="), LayerName))
			{
				new (LayerNames) FName(*LayerName);

				while(*SourceText && (!appIsWhitespace(*SourceText)))
				{
					++SourceText;
				}
				ParseNext(&SourceText);
				check(*SourceText);

				i = 0;
				while( appIsDigit(*SourceText) ) 
				{
					if( i < NumVertices )
					{
						(WeightmapData(LayerIdx))(i++) = (BYTE)appAtoi(SourceText);
						while( appIsDigit(*SourceText) ) 
						{
							SourceText++;
						}
					}
					ParseNext(&SourceText);
				} 

				if( i != NumVertices )
				{
					Warn->Logf( *LocalizeError(TEXT("CustomProperties(LanscapeComponent Weightmap) Syntax Error"), TEXT("Core")));
				}
				LayerIdx++;
			}
			else
			{
				break;
			}
		}

		InitWeightmapData(LayerNames, WeightmapData);
	}
}

void ALandscapeProxy::UpdateLandscapeActor(ALandscape* Landscape)
{
	if (bIsProxy)
	{
		if (!Landscape)
		{
			if (GWorld)
			{
				for (FActorIterator It; It; ++It)
				{
					Landscape = Cast<ALandscape>(*It);
					if (Landscape &&  IsValidLandscapeActor(Landscape) )
					{
						LandscapeActor = Landscape;
						break;
					}
				}
			}
			else // Maybe Proxy and LandscapeActor is in same level (still loading...)
			{
				Landscape = LoadObject<ALandscape>(NULL, *LandscapeActorName, NULL, LOAD_None, NULL);
			}
		}

		if (Landscape &&  IsValidLandscapeActor(Landscape) )
		{
			LandscapeActor = Landscape;
			GetSharedProperties(LandscapeActor);
			LandscapeActor->Proxies.Add(this);
		}
	}
}

UBOOL ALandscapeProxy::IsValidLandscapeActor(ALandscape* Landscape)
{
	if (bIsProxy && Landscape)
	{
		if (Landscape->GetPathName() == LandscapeActorName)
		{
			if (LandscapeComponents.Num())
			{
				ULandscapeComponent* Comp = LandscapeComponents(0);
				if (Comp->ComponentSizeQuads == Landscape->ComponentSizeQuads && Comp->NumSubsections == Landscape->NumSubsections
					&& Comp->SubsectionSizeQuads == Landscape->SubsectionSizeQuads)
				{
					return TRUE;
				}
			}
			return TRUE;
		}
	}
	return FALSE;
}

#endif //WITH_EDITOR