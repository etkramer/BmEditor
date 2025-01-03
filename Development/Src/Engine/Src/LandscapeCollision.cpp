/*=============================================================================
LandscapeCollision.cpp: Landscape collision
Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "UnTerrain.h"
#include "LandscapeDataAccess.h"
#include "LandscapeRender.h"
#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

UBOOL ALandscapeProxy::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	return (TraceFlags & TRACE_Terrain);
}

void ULandscapeHeightfieldCollisionComponent::Attach()
{
	if( GWorld )
	{
		GWorld->LandscapeComponentCount++;
	}
	Super::Attach();
}

void ULandscapeHeightfieldCollisionComponent::Detach(UBOOL bWillReattach)
{
	if( GWorld )
	{
		--GWorld->LandscapeComponentCount;
	}
	Super::Detach(bWillReattach);
}

void ULandscapeHeightfieldCollisionComponent::UpdateBounds()
{
	Bounds = CachedBoxSphereBounds;
}

void ULandscapeHeightfieldCollisionComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(FTranslationMatrix(FVector(SectionBaseX,SectionBaseY,0)) * ParentToWorld);
}

/** Create a PhysX Heightfield shape and NxActor for Unreal and rigid body collision. */
void ULandscapeHeightfieldCollisionComponent::InitComponentRBPhys(UBOOL bFixed)
{
	// If no physics scene, or we already have a BodyInstance, do nothing.
	if( !BlockRigidBody || !GWorld->RBPhysScene || BodyInstance || bDisableAllRigidBody )
	{
		return;
	}

#if WITH_NOVODEX

	INT CollisionSizeVerts = CollisionSizeQuads+1;

	check(bFixed);

	// Make transform for this landscape component NxActor
	FMatrix LandscapeCompTM = LocalToWorld;
	FVector LandscapeScale = LandscapeCompTM.ExtractScaling();
	FVector TerrainY = LandscapeCompTM.GetAxis(1);
	FVector TerrainZ = LandscapeCompTM.GetAxis(2);
	LandscapeCompTM.SetAxis(2, -TerrainY);
	LandscapeCompTM.SetAxis(1, TerrainZ);
	NxMat34 PhysXLandscapeCompTM = U2NTransform(LandscapeCompTM);

	check(GEngine->DefaultPhysMaterial);

	// Create an RB_BodyInstance for this terrain component and store a pointer to the NxActor in it.
	BodyInstance = GWorld->InstanceRBBody(NULL);
	BodyInstance->BodyData = NULL;
	BodyInstance->OwnerComponent = this;
	BodyInstance->SceneIndex = GWorld->RBPhysScene->NovodexSceneIndex;

	// Doesn't really matter what index this is - as long as its not the default.
	INT DefPhysMaterialIndex = GWorld->RBPhysScene->FindPhysMaterialIndex( GEngine->DefaultPhysMaterial );
	INT HoleMaterial = DefPhysMaterialIndex + 1;

	// If we have not created a heightfield yet - do it now.
	if(!RBHeightfield)
	{
		WORD* Heights = (WORD*)CollisionHeightData.Lock(LOCK_READ_ONLY);
		check(CollisionHeightData.GetElementCount()==Square(CollisionSizeVerts));

		TArray<NxHeightFieldSample> Samples;
		Samples.AddZeroed(Square(CollisionSizeVerts));

		for(INT RowIndex = 0; RowIndex < CollisionSizeVerts; RowIndex++)
		{
			for(INT ColIndex = 0; ColIndex < CollisionSizeVerts; ColIndex++)
			{
				INT SrcSampleIndex = (ColIndex * CollisionSizeVerts) + RowIndex;
				INT DstSampleIndex = (RowIndex * CollisionSizeVerts) + ColIndex;

				NxHeightFieldSample& Sample = Samples(DstSampleIndex);
				Sample.height = Clamp<SWORD>(((INT)Heights[SrcSampleIndex]-32768), -32678, 32767);
				
				// TODO: edge turning, dominant physical material, holes.
				Sample.tessFlag = 0;
				Sample.materialIndex0 = DefPhysMaterialIndex;
				Sample.materialIndex1 = DefPhysMaterialIndex;
			}
		}

		NxHeightFieldDesc HFDesc;
		HFDesc.nbColumns		= CollisionSizeVerts;
		HFDesc.nbRows			= CollisionSizeVerts;
		HFDesc.samples			= Samples.GetData();
		HFDesc.sampleStride		= sizeof(NxU32);
		HFDesc.flags			= NX_HF_NO_BOUNDARY_EDGES;

		RBHeightfield = GNovodexSDK->createHeightField(HFDesc);

		CollisionHeightData.Unlock();
	}
	check(RBHeightfield);

	NxHeightFieldShapeDesc LandscapeComponentShapeDesc;
	LandscapeComponentShapeDesc.heightField		= RBHeightfield;
	LandscapeComponentShapeDesc.shapeFlags		= NX_SF_FEATURE_INDICES | NX_SF_VISUALIZATION;
	LandscapeComponentShapeDesc.heightScale		= LandscapeScale.Z * TERRAIN_ZSCALE * U2PScale;
	LandscapeComponentShapeDesc.rowScale		= LandscapeScale.Y * CollisionScale * U2PScale;
	LandscapeComponentShapeDesc.columnScale		= -LandscapeScale.X * CollisionScale * U2PScale;
	LandscapeComponentShapeDesc.meshFlags		= 0;
	LandscapeComponentShapeDesc.materialIndexHighBits = 0;
	LandscapeComponentShapeDesc.holeMaterial	= HoleMaterial;
	LandscapeComponentShapeDesc.groupsMask		= CreateGroupsMask(RBCC_Default, NULL);
	LandscapeComponentShapeDesc.group			= SHAPE_GROUP_LANDSCAPE;

	// Create actor description and instance it.
	NxActorDesc LandscapeActorDesc;
	LandscapeActorDesc.shapes.pushBack(&LandscapeComponentShapeDesc);
	LandscapeActorDesc.globalPose = PhysXLandscapeCompTM;

	// Create the actual NxActor using the mesh collision shape.
	NxScene* NovodexScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
	check(NovodexScene);

	NxActor* LandscapeNxActor = NovodexScene->createActor(LandscapeActorDesc);

	if(LandscapeNxActor)
	{
		BodyInstance->BodyData = (FPointer)LandscapeNxActor;
		LandscapeNxActor->userData = BodyInstance;
	}
	else
	{
		debugf(TEXT("ULandscapeHeightfieldCollisionComponent::InitComponentRBPhys : Could not create NxActor"));
	}
#endif
}

void ULandscapeHeightfieldCollisionComponent::BeginDestroy()
{
#if WITH_NOVODEX
	// Free the heightfield data.
	if(RBHeightfield)
	{
		GNovodexPendingKillHeightfield.AddItem(RBHeightfield);
		RBHeightfield = NULL;
	}
#endif
#if WITH_EDITOR
	if( GIsEditor && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		ALandscape* Landscape = GetLandscapeActor();
		if (Landscape)
		{
			Landscape->XYtoCollisionComponentMap.Remove(ALandscape::MakeKey(SectionBaseX,SectionBaseY));
		}
	}
#endif

	Super::BeginDestroy();
}

void ULandscapeHeightfieldCollisionComponent::RecreateHeightfield(UBOOL bUpdateAddCollision/*= TRUE*/)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		TermComponentRBPhys(NULL);

#if WITH_NOVODEX
		// Free the existing heightfield data.
		if(RBHeightfield)
		{
			GNovodexPendingKillHeightfield.AddItem(RBHeightfield);
			RBHeightfield = NULL;
		}
#endif
#if WITH_EDITOR
		if (bUpdateAddCollision)
		{
			UpdateAddCollisions(TRUE);
		}
#endif

		InitComponentRBPhys(TRUE);
	}
}

void ULandscapeHeightfieldCollisionComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	CollisionHeightData.Serialize(Ar,this);
}

#if WITH_EDITOR
void ULandscapeHeightfieldCollisionComponent::PostEditUndo()
{
	Super::PostEditUndo();
	// Reinitialize physics after undo
	if (CollisionSizeQuads > 0)
	{
		RecreateHeightfield(FALSE);
	}
}
#endif

/*-----------------------------------------------------------------------------
	World-Landscape line/ point checking
-----------------------------------------------------------------------------*/
UBOOL UWorld::LandscapeLineCheck( FCheckResult& Hit, AActor* Owner, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags )
{	
	UBOOL bHit = FALSE;

#if WITH_NOVODEX
	if( GWorld->RBPhysScene )
	{
		FCheckResult TotalResult(1.f);

		// Zero-extent is handled by ULandscapeHeightfieldCollisionComponent::LineCheck
		if( !Extent.IsZero() )
		{
#define EXTRA_TRACE_DIST 5.f
			FVector StartEnd = (End-Start);
			FLOAT Distance = StartEnd.Size();
			if( Distance > SMALL_NUMBER )
			{
				FVector NxExtent = Extent * U2PScale;
				FLOAT InvDistance = 1.f / Distance;
				FVector Direction = StartEnd * InvDistance;

				// We actually start our trace EXTRA_TRACE_DIST from the requested Start location
				// This ensures we don't fall through the terrain should Unreal walking code start the trace
				// infinitesimally close to the surface of the terrain.
				FVector ModifiedStart = (Start - EXTRA_TRACE_DIST * Direction);

				FVector ModifiedStartEnd = (End-ModifiedStart);
				FVector NxStartEnd = ModifiedStartEnd * U2PScale;
				FVector NxModifiedStart = ModifiedStart * U2PScale;

				NxBox testBox;
				testBox.setEmpty();
				testBox.center	= U2NVectorCopy(NxModifiedStart);
				testBox.extents	= U2NVectorCopy(NxExtent);

				NxSweepQueryHit result;

				// Mask out all except Landscape Heightfield shapes
				NxU32 GroupMask = (1<<SHAPE_GROUP_LANDSCAPE);
				if( GWorld->RBPhysScene->GetNovodexPrimaryScene()->linearOBBSweep(testBox, U2NVectorCopy(NxStartEnd), NX_SF_STATICS, NULL, 1, &result, NULL, GroupMask) > 0 )
				{
					if( result.t <= 1.f )
					{
						if( (EXTRA_TRACE_DIST+Distance) * result.t <= EXTRA_TRACE_DIST )
						{
							// Hit before the start of the trace. Return a hit time of 0.
							TotalResult.Time = 0.f;
							TotalResult.Location = Start;						
						}
						else
						{
							TotalResult.Location = ModifiedStart + ModifiedStartEnd * result.t;
							TotalResult.Time = (TotalResult.Location - Start).Size() * InvDistance;
						}

						TotalResult.Component = ((URB_BodyInstance*)result.hitShape->getActor().userData)->OwnerComponent;
						TotalResult.Actor = TotalResult.Component->GetOwner();
						TotalResult.Normal = N2UVectorCopy(result.normal).SafeNormal();
						Hit = TotalResult;
						bHit = TRUE;
					}
				}
			}
		}
	}
#endif

	// return hit condition
	return !bHit;
}

UBOOL ULandscapeHeightfieldCollisionComponent::LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	// Non-zero extent checks are handled by UWorld::LandscapeLineCheck, as the current PhysX API cannot do a swept box check against a single PhysX shape.
#if WITH_NOVODEX
	if( BodyInstance && Extent.IsZero() )
	{
		FVector NxStart = Start * U2PScale;
		FVector NxStartEnd = (End-Start) * U2PScale;
		FLOAT NxTraceDistance = NxStartEnd.Size();
		if( NxTraceDistance > SMALL_NUMBER )
		{
			FLOAT NxInvTraceDistance = 1.f / NxTraceDistance;
			FVector NxDirection = NxStartEnd * NxInvTraceDistance;

			NxRay Ray;
			Ray.orig = U2NVectorCopy(NxStart);
			Ray.dir = U2NVectorCopy(NxDirection);

			NxRaycastHit RaycastHit;

			NxActor* HeightfieldActor = (NxActor*)BodyInstance->BodyData;
			check(HeightfieldActor->getNbShapes() == 1);
			NxShape* HeightfieldShape = (HeightfieldActor->getShapes())[0];
			check(HeightfieldShape);

			NxU32 hintFlags = NX_RAYCAST_NORMAL | NX_RAYCAST_IMPACT | NX_RAYCAST_DISTANCE;
			if( TraceFlags & TRACE_Material )
			{
				hintFlags |= NX_RAYCAST_MATERIAL;
			}

			if( HeightfieldShape->raycast(Ray, NxTraceDistance, hintFlags, RaycastHit, (TraceFlags & TRACE_StopAtAnyHit) ? TRUE : FALSE ) )
			{
				if( RaycastHit.distance <= NxTraceDistance )
				{
					Result.Actor		= Owner;
					Result.Component	= this;
					Result.Time			= RaycastHit.distance * NxInvTraceDistance;
					Result.Location		= N2UVectorCopy(RaycastHit.worldImpact) * P2UScale;
					Result.Normal		= N2UVectorCopy(RaycastHit.worldNormal).SafeNormal();
					return FALSE;
				}
			}
		}
	}
#endif
	return TRUE;
}

UBOOL ULandscapeHeightfieldCollisionComponent::PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags)
{
#if WITH_NOVODEX
	if( BodyInstance )
	{
		NxActor* HeightfieldActor = (NxActor*)BodyInstance->BodyData;
		check(HeightfieldActor->getNbShapes() == 1);
		NxShape* HeightfieldShape = (HeightfieldActor->getShapes())[0];
		check(HeightfieldShape);

		NxBounds3 WorldBounds;
		WorldBounds.set(U2NVectorCopy((Location-Extent) * U2PScale), U2NVectorCopy((Location+Extent) * U2PScale));

		if( HeightfieldShape->checkOverlapAABB(WorldBounds) )
		{
			Result.Actor		= Owner;
			Result.Component	= this;
			Result.Time			= 0.f;
			Result.Location		= Location;
			return FALSE;
		}
	}
#endif
	return TRUE;
}

#if WITH_EDITOR
void ULandscapeHeightfieldCollisionComponent::SetupActor()
{
	if( GIsEditor && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		ALandscape* Landscape = GetLandscapeActor();
		if (Landscape)
		{
			Landscape->XYtoCollisionComponentMap.Set(ALandscape::MakeKey(SectionBaseX,SectionBaseY), this);
		}
	}
}

void ULandscapeHeightfieldCollisionComponent::PostLoad()
{
	Super::PostLoad();
	SetupActor();
}

void ULandscapeHeightfieldCollisionComponent::PostRename()
{
	Super::PostRename();
	SetupActor();
}

void ULandscapeHeightfieldCollisionComponent::PostEditImport()
{
	Super::PostEditImport();
	SetupActor();
}

void ULandscapeHeightfieldCollisionComponent::ExportCustomProperties(FOutputDevice& Out, UINT Indent)
{
	WORD* Heights = (WORD*)CollisionHeightData.Lock(LOCK_READ_ONLY);
	INT NumHeights = Square(CollisionSizeQuads+1);
	check(CollisionHeightData.GetElementCount()==NumHeights);

	Out.Logf( TEXT("%sCustomProperties CollisionHeightData "), appSpc(Indent));
	for( INT i=0;i<NumHeights;i++ )
	{
		Out.Logf( TEXT("%d "), Heights[i] );
	}

	CollisionHeightData.Unlock();
	Out.Logf( TEXT("\r\n") );
}

void ULandscapeHeightfieldCollisionComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	if(ParseCommand(&SourceText,TEXT("CollisionHeightData")))
	{
		INT NumHeights = Square(CollisionSizeQuads+1);

		CollisionHeightData.Lock(LOCK_READ_WRITE);
		WORD* Heights = (WORD*)CollisionHeightData.Realloc(NumHeights);
		appMemzero(Heights, sizeof(WORD)*NumHeights);

		ParseNext(&SourceText);
		INT i = 0;
		while( appIsDigit(*SourceText) ) 
		{
			if( i < NumHeights )
			{
				Heights[i++] = appAtoi(SourceText);
				while( appIsDigit(*SourceText) ) 
				{
					SourceText++;
				}
			}
			
			ParseNext(&SourceText);
		} 

		CollisionHeightData.Unlock();

		if( i != NumHeights )
		{
			Warn->Logf( *LocalizeError(TEXT("CustomProperties Syntax Error"), TEXT("Core")));
		}
	}
}

void ALandscape::UpdateAllAddCollisions()
{
	//XYtoAddCollisionMap.Empty();
	TArray<ULandscapeHeightfieldCollisionComponent*> AllCollisionComponents;
	GetAllEditableComponents(NULL, &AllCollisionComponents);
	for(INT ComponentIndex = 0; ComponentIndex < AllCollisionComponents.Num(); ComponentIndex++ )
	{
		ULandscapeHeightfieldCollisionComponent* Comp = AllCollisionComponents(ComponentIndex);
		if( Comp )
		{
			Comp->UpdateAddCollisions();
		}
	}
}

void ULandscapeHeightfieldCollisionComponent::UpdateAddCollisions(UBOOL bForceUpdate /*= FALSE*/)
{
	ALandscape* Landscape = GetLandscapeActor();
	if (Landscape)
	{
		QWORD NeighborsKeys[8] = 
		{
			ALandscape::MakeKey(SectionBaseX-Landscape->ComponentSizeQuads, SectionBaseY-Landscape->ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX,								SectionBaseY-Landscape->ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX+Landscape->ComponentSizeQuads, SectionBaseY-Landscape->ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX-Landscape->ComponentSizeQuads, SectionBaseY),
			ALandscape::MakeKey(SectionBaseX+Landscape->ComponentSizeQuads, SectionBaseY),
			ALandscape::MakeKey(SectionBaseX-Landscape->ComponentSizeQuads, SectionBaseY+Landscape->ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX,								SectionBaseY+Landscape->ComponentSizeQuads),
			ALandscape::MakeKey(SectionBaseX+Landscape->ComponentSizeQuads, SectionBaseY+Landscape->ComponentSizeQuads)
		};

		// Search for Neighbors...
		for (INT i = 0; i < 8; ++i)
		{
			if (!Landscape->XYtoCollisionComponentMap.FindRef(NeighborsKeys[i]))
			{
				Landscape->UpdateAddCollision(NeighborsKeys[i], bForceUpdate);
			}
			else
			{
				Landscape->XYtoAddCollisionMap.Remove(NeighborsKeys[i]);
			}
		}
	}
}

void ALandscape::UpdateAddCollision(QWORD LandscapeKey, UBOOL bForceUpdate /*= FALSE*/ )
{
	INT SectionBaseX, SectionBaseY;
	ALandscape::UnpackKey(LandscapeKey, SectionBaseX, SectionBaseY);

	FLandscapeAddCollision* AddCollision = XYtoAddCollisionMap.Find(LandscapeKey);
	if (!AddCollision)
	{
		AddCollision = &XYtoAddCollisionMap.Set(LandscapeKey, FLandscapeAddCollision());
	}
	else if (!bForceUpdate)
	{
		return;
	}
	check(AddCollision);

	// 8 Neighbors... 
	// 0 1 2 
	// 3   4
	// 5 6 7
	QWORD NeighborsKeys[8] = 
	{
		ALandscape::MakeKey(SectionBaseX-ComponentSizeQuads, SectionBaseY-ComponentSizeQuads),
		ALandscape::MakeKey(SectionBaseX,					SectionBaseY-ComponentSizeQuads),
		ALandscape::MakeKey(SectionBaseX+ComponentSizeQuads, SectionBaseY-ComponentSizeQuads),
		ALandscape::MakeKey(SectionBaseX-ComponentSizeQuads, SectionBaseY),
		ALandscape::MakeKey(SectionBaseX+ComponentSizeQuads, SectionBaseY),
		ALandscape::MakeKey(SectionBaseX-ComponentSizeQuads, SectionBaseY+ComponentSizeQuads),
		ALandscape::MakeKey(SectionBaseX,					SectionBaseY+ComponentSizeQuads),
		ALandscape::MakeKey(SectionBaseX+ComponentSizeQuads, SectionBaseY+ComponentSizeQuads)
	};

	ULandscapeHeightfieldCollisionComponent* NeighborCollisions[8];
	// Search for Neighbors...
	for (INT i = 0; i < 8; ++i)
	{
		NeighborCollisions[i] = XYtoCollisionComponentMap.FindRef(NeighborsKeys[i]);
	}

	BYTE CornerSet = 0;
	BYTE OriginalSet = 0;
	WORD HeightCorner[4];

	// Corner Cases...
	if (NeighborCollisions[0])
	{
		WORD* Heights = (WORD*)NeighborCollisions[0]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[0]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[ CollisionSizeVerts-1 + (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1;
		NeighborCollisions[0]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[2])
	{
		WORD* Heights = (WORD*)NeighborCollisions[2]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[2]->CollisionSizeQuads + 1;
		HeightCorner[1] = Heights[ (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1 << 1;
		NeighborCollisions[2]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[5])
	{
		WORD* Heights = (WORD*)NeighborCollisions[5]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[5]->CollisionSizeQuads + 1;
		HeightCorner[2] = Heights[ (CollisionSizeVerts-1) ];
		CornerSet |= 1 << 2;
		NeighborCollisions[5]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[7])
	{
		WORD* Heights = (WORD*)NeighborCollisions[7]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[7]->CollisionSizeQuads + 1;
		HeightCorner[3] = Heights[ 0 ];
		CornerSet |= 1 << 3;
		NeighborCollisions[7]->CollisionHeightData.Unlock();
	}

	// Other cases...
	if (NeighborCollisions[1])
	{
		WORD* Heights = (WORD*)NeighborCollisions[1]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[1]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[ (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1;
		HeightCorner[1] = Heights[ CollisionSizeVerts-1 + (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1 << 1;
		NeighborCollisions[1]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[3])
	{
		WORD* Heights = (WORD*)NeighborCollisions[3]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[3]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[ (CollisionSizeVerts-1) ];
		CornerSet |= 1;
		HeightCorner[2] = Heights[ CollisionSizeVerts-1 + (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1 << 2;
		NeighborCollisions[3]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[4])
	{
		WORD* Heights = (WORD*)NeighborCollisions[4]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[4]->CollisionSizeQuads + 1;
		HeightCorner[1] = Heights[ 0 ];
		CornerSet |= 1 << 1;
		HeightCorner[3] = Heights[ (CollisionSizeVerts-1)*CollisionSizeVerts ];
		CornerSet |= 1 << 3;
		NeighborCollisions[4]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[6])
	{
		WORD* Heights = (WORD*)NeighborCollisions[6]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		INT CollisionSizeVerts = NeighborCollisions[6]->CollisionSizeQuads + 1;
		HeightCorner[2] = Heights[ 0 ];
		CornerSet |= 1 << 2;
		HeightCorner[3] = Heights[ (CollisionSizeVerts-1) ];
		CornerSet |= 1 << 3;
		NeighborCollisions[6]->CollisionHeightData.Unlock();
	}

	OriginalSet = CornerSet;

	// Fill unset values
	// First iteration only for valid values distance 1 propagation
	// Second iteration fills left ones...
	FillCornerValues(CornerSet, HeightCorner);
	check(CornerSet == 15);

	// Transform Height to Vectors...
	AddCollision->Corners[0] = LocalToWorld().TransformFVector( FVector(SectionBaseX,					SectionBaseY,					((FLOAT)HeightCorner[0] - 32768.f) * LANDSCAPE_ZSCALE ) );
	AddCollision->Corners[1] = LocalToWorld().TransformFVector( FVector(SectionBaseX+ComponentSizeQuads,SectionBaseY,					((FLOAT)HeightCorner[1] - 32768.f) * LANDSCAPE_ZSCALE ) );
	AddCollision->Corners[2] = LocalToWorld().TransformFVector( FVector(SectionBaseX,					SectionBaseY+ComponentSizeQuads,((FLOAT)HeightCorner[2] - 32768.f) * LANDSCAPE_ZSCALE ) );
	AddCollision->Corners[3] = LocalToWorld().TransformFVector( FVector(SectionBaseX+ComponentSizeQuads,SectionBaseY+ComponentSizeQuads,((FLOAT)HeightCorner[3] - 32768.f) * LANDSCAPE_ZSCALE ) );
}

#endif


IMPLEMENT_CLASS(ULandscapeHeightfieldCollisionComponent);
