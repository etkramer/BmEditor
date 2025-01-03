/*================================================================================
	LandscapeEdMode.cpp: Landscape editing mode
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "UnrealEd.h"
#include "UnObjectTools.h"
#include "LandscapeEdMode.h"
#include "ScopedTransaction.h"
#include "EngineTerrainClasses.h"
#include "LandscapeEdit.h"
#include "LandscapeRender.h"

#if WITH_MANAGED_CODE
#include "LandscapeEditWindowShared.h"
#endif

namespace
{
	FLandscapeAddCollision* GLandscapeRenderAddCollision = NULL;
}

//
// FLandscapeHeightCache
//
template<class Accessor, typename AccessorType>
struct TLandscapeEditCache
{
	TLandscapeEditCache(Accessor& InDataAccess)
	:	DataAccess(InDataAccess)
	,	Valid(FALSE)
	{
	}

	void CacheData( INT X1, INT Y1, INT X2, INT Y2 )
	{
		if( !Valid )
		{
			if (Accessor::bUseInterp)
			{
				ValidX1 = CachedX1 = X1;
				ValidY1 = CachedY1 = Y1;
				ValidX2 = CachedX2 = X2;
				ValidY2 = CachedY2 = Y2;

				DataAccess.GetData( ValidX1, ValidY1, ValidX2, ValidY2, CachedData );
				check(ValidX1 <= ValidX2 && ValidY1 <= ValidY2);
			}
			else
			{
				CachedX1 = X1;
				CachedY1 = Y1;
				CachedX2 = X2;
				CachedY2 = Y2;

				DataAccess.GetDataFast( CachedX1, CachedY1, CachedX2, CachedY2, CachedData );
			}

			Valid = TRUE;
		}
		else
		{
			// Extend the cache area if needed
			if( X1 < CachedX1 )
			{
				if (Accessor::bUseInterp)
				{
					INT x1 = X1;
					INT x2 = ValidX1;
					INT y1 = Min<INT>(Y1,CachedY1);
					INT y2 = Max<INT>(Y2,CachedY2);

					DataAccess.GetData( x1, y1, x2, y2, CachedData );
					ValidX1 = Min<INT>(x1,ValidX1);
				}
				else
				{
					DataAccess.GetDataFast( X1, Min<INT>(Y1,CachedY1), CachedX1-1, Max<INT>(Y2,CachedY2), CachedData );
				}
			}
			CachedX1 = Min<INT>(X1,CachedX1);

			if( X2 > CachedX2 )
			{
				if (Accessor::bUseInterp)
				{
					INT x1 = ValidX2;
					INT x2 = X2;
					INT y1 = Min<INT>(Y1,CachedY1);
					INT y2 = Max<INT>(Y2,CachedY2);

					DataAccess.GetData( x1, y1, x2, y2, CachedData );
					ValidX2 = Max<INT>(x2,ValidX2);
				}
				else
				{
					DataAccess.GetDataFast( CachedX2+1, Min<INT>(Y1,CachedY1), X2, Max<INT>(Y2,CachedY2), CachedData );
				}
			}
			CachedX2 = Max<INT>(X2,CachedX2);

			if( Y1 < CachedY1 )
			{
				if (Accessor::bUseInterp)
				{
					INT x1 = CachedX1;
					INT x2 = CachedX2;
					INT y1 = Y1;
					INT y2 = ValidY1;

					DataAccess.GetData( x1, y1, x2, y2, CachedData );
					ValidY1 = Min<INT>(y1,ValidY1);
				}
				else
				{
					DataAccess.GetDataFast( CachedX1, Y1, CachedX2, CachedY1-1, CachedData );
				}
			}
			CachedY1 = Min<INT>(Y1,CachedY1);

			if( Y2 > CachedY2 )
			{
				if (Accessor::bUseInterp)
				{
					INT x1 = CachedX1;
					INT x2 = CachedX2;
					INT y1 = ValidY2;
					INT y2 = Y2;

					DataAccess.GetData( x1, y1, x2, y2, CachedData );
					ValidY2 = Max<INT>(y2,ValidY2);
				}
				else
				{
					DataAccess.GetDataFast( CachedX1, CachedY2+1, CachedX2, Y2, CachedData );
				}
			}
			CachedY2 = Max<INT>(Y2,CachedY2);
		}	
	}

	AccessorType* GetValueRef(INT LandscapeX, INT LandscapeY)
	{
		return CachedData.Find(ALandscape::MakeKey(LandscapeX,LandscapeY));
	}

	void SetValue(INT LandscapeX, INT LandscapeY, AccessorType Value)
	{
		CachedData.Set(ALandscape::MakeKey(LandscapeX,LandscapeY), Value);
	}

	void GetCachedData(INT X1, INT Y1, INT X2, INT Y2, TArray<AccessorType>& OutData)
	{
		INT NumSamples = (1+X2-X1)*(1+Y2-Y1);
		OutData.Empty(NumSamples);
		OutData.Add(NumSamples);

		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				AccessorType* Ptr = GetValueRef(X,Y);
				if( Ptr )
				{
					OutData((X-X1) + (Y-Y1)*(1+X2-X1)) = *Ptr;
				}
			}
		}
	}

	void SetCachedData(INT X1, INT Y1, INT X2, INT Y2, TArray<AccessorType>& Data)
	{
		// Update cache
		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				SetValue( X, Y, Data((X-X1) + (Y-Y1)*(1+X2-X1)) );
			}
		}

		// Update real data
		DataAccess.SetData( X1, Y1, X2, Y2, &Data(0) );
	}

	void Flush()
	{
		DataAccess.Flush();
	}

protected:
	Accessor& DataAccess;
private:
	TMap<QWORD, AccessorType> CachedData;
	UBOOL Valid;

	INT CachedX1;
	INT CachedY1;
	INT CachedX2;
	INT CachedY2;

	// To store valid region....
	INT ValidX1, ValidX2, ValidY1, ValidY2;
};

//
// FHeightmapAccessor
//
template<UBOOL bInUseInterp>
struct FHeightmapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FHeightmapAccessor( ALandscape* InLandscape )
	{
		LandscapeEdit = new FLandscapeEditDataInterface(InLandscape);
	}

	// accessors
	void GetData(INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, WORD>& Data)
	{
		LandscapeEdit->GetHeightData( X1, Y1, X2, Y2, Data);
	}

	void GetDataFast(INT X1, INT Y1, INT X2, INT Y2, TMap<QWORD, WORD>& Data)
	{
		LandscapeEdit->GetHeightDataFast( X1, Y1, X2, Y2, Data);
	}

	void SetData(INT X1, INT Y1, INT X2, INT Y2, const WORD* Data )
	{
		if (LandscapeEdit->GetComponentsInRegion(X1, Y1, X2, Y2, &ChangedComponents))
		{
			LandscapeEdit->SetHeightData( X1, Y1, X2, Y2, Data, 0, TRUE);
		}
		else
		{
			ChangedComponents.Empty();
		}
	}

	void Flush()
	{
		LandscapeEdit->Flush();
	}

	virtual ~FHeightmapAccessor()
	{
		delete LandscapeEdit;
		LandscapeEdit = NULL;

		// Update the bounds for the components we edited
		for(TSet<ULandscapeComponent*>::TConstIterator It(ChangedComponents);It;++It)
		{
			(*It)->UpdateCachedBounds();
			(*It)->ConditionalUpdateTransform();
		}
	}

private:
	FLandscapeEditDataInterface* LandscapeEdit;
	TSet<ULandscapeComponent*> ChangedComponents;
};

struct FLandscapeHeightCache : public TLandscapeEditCache<FHeightmapAccessor<TRUE>,WORD>
{
	typedef WORD DataType;
	static WORD ClampValue( INT Value ) { return Clamp(Value, 0, 65535); }

	FHeightmapAccessor<TRUE> HeightmapAccessor;

	FLandscapeHeightCache(const FLandscapeToolTarget& InTarget)
	:	HeightmapAccessor(InTarget.Landscape)
	,	TLandscapeEditCache(HeightmapAccessor)
	{
	}
};

//
// FAlphamapAccessor
//
template<UBOOL bInUseInterp>
struct FAlphamapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FAlphamapAccessor( ALandscape* InLandscape, FName InLayerName )
		:	LandscapeEdit(InLandscape)
		,	LayerName(InLayerName)
		,	bBlendWeight(TRUE)
	{
		// should be no Layer change during FAlphamapAccessor lifetime...
		if (InLandscape && LayerName != NAME_None)
		{
			for (INT LayerIdx = 0; LayerIdx < InLandscape->LayerInfos.Num(); LayerIdx++)
	{
				if (InLandscape->LayerInfos(LayerIdx).LayerName == LayerName)
				{
					bBlendWeight = !InLandscape->LayerInfos(LayerIdx).bNoWeightBlend;
					break;
				}
			}
		}
	}

	void GetData(INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, BYTE>& Data)
	{
		LandscapeEdit.GetWeightData(LayerName, X1, Y1, X2, Y2, Data);
	}

	void GetDataFast(INT X1, INT Y1, INT X2, INT Y2, TMap<QWORD, BYTE>& Data)
	{
		LandscapeEdit.GetWeightDataFast(LayerName, X1, Y1, X2, Y2, Data);
	}

	void SetData(INT X1, INT Y1, INT X2, INT Y2, const BYTE* Data )
	{
		if (LandscapeEdit.GetComponentsInRegion(X1, Y1, X2, Y2))
		{
			LandscapeEdit.SetAlphaData(LayerName, X1, Y1, X2, Y2, Data, 0, bBlendWeight);
		}
	}

	void Flush()
	{
		LandscapeEdit.Flush();
	}

private:
	FLandscapeEditDataInterface LandscapeEdit;
	FName LayerName;
	BOOL bBlendWeight;
};

struct FLandscapeAlphaCache : public TLandscapeEditCache<FAlphamapAccessor<TRUE>,BYTE>
{
	typedef BYTE DataType;
	static BYTE ClampValue( INT Value ) { return Clamp(Value, 0, 255); }

	FAlphamapAccessor<TRUE> AlphamapAccessor;

	FLandscapeAlphaCache(const FLandscapeToolTarget& InTarget)
		:	AlphamapAccessor(InTarget.Landscape, InTarget.LayerName)
		,	TLandscapeEditCache(AlphamapAccessor)
	{
	}
};

//
// FFullWeightmapAccessor
//
template<UBOOL bInUseInterp>
struct FFullWeightmapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FFullWeightmapAccessor( ALandscape* InLandscape)
		:	LandscapeEdit(InLandscape)
	{
	}
	void GetData(INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, TArray<BYTE>>& Data)
	{
		// Do not Support for interpolation....
		check(FALSE && TEXT("Do not support interpolation for FullWeightmapAccessor for now"));
		//LandscapeEdit.GetWeightData(NAME_None, X1, Y1, X2, Y2, Data);
	}

	void GetDataFast(INT X1, INT Y1, INT X2, INT Y2, TMap<QWORD, TArray<BYTE>>& Data)
	{
		LandscapeEdit.GetWeightDataFast(NAME_None, X1, Y1, X2, Y2, Data);
	}

	void SetData(INT X1, INT Y1, INT X2, INT Y2, const BYTE* Data)
	{
		if (LandscapeEdit.GetComponentsInRegion(X1, Y1, X2, Y2))
		{
			LandscapeEdit.SetAlphaData(NAME_None, X1, Y1, X2, Y2, Data, 0, FALSE);
		}
	}

	void Flush()
	{
		LandscapeEdit.Flush();
	}

private:
	FLandscapeEditDataInterface LandscapeEdit;
};

struct FLandscapeFullWeightCache : public TLandscapeEditCache<FFullWeightmapAccessor<FALSE>,TArray<BYTE>>
{
	typedef TArray<BYTE> DataType;

	FFullWeightmapAccessor<FALSE> WeightmapAccessor;

	FLandscapeFullWeightCache(const FLandscapeToolTarget& InTarget)
		:	WeightmapAccessor(InTarget.Landscape)
		,	TLandscapeEditCache(WeightmapAccessor)
	{
	}

	// Only for all weight case... the accessor type should be TArray<BYTE>
	void GetCachedData(INT X1, INT Y1, INT X2, INT Y2, TArray<BYTE>& OutData, INT ArraySize)
	{
		INT NumSamples = (1+X2-X1)*(1+Y2-Y1) * ArraySize;
		OutData.Empty(NumSamples);
		OutData.Add(NumSamples);

		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				TArray<BYTE>* Ptr = GetValueRef(X,Y);
				if( Ptr )
				{
					for (INT Z = 0; Z < ArraySize; Z++)
					{
						OutData(( (X-X1) + (Y-Y1)*(1+X2-X1)) * ArraySize + Z) = (*Ptr)(Z);
					}
				}
			}
		}
	}

	// Only for all weight case... the accessor type should be TArray<BYTE>
	void SetCachedData(INT X1, INT Y1, INT X2, INT Y2, TArray<BYTE>& Data, INT ArraySize)
	{
		// Update cache
		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				TArray<BYTE> Value;
				Value.Empty(ArraySize);
				Value.Add(ArraySize);
				for ( INT Z=0; Z < ArraySize; Z++)
				{
					Value(Z) = Data( ((X-X1) + (Y-Y1)*(1+X2-X1)) * ArraySize + Z);
				}
				SetValue( X, Y, Value );
			}
		}

		// Update real data
		DataAccess.SetData( X1, Y1, X2, Y2, &Data(0) );
	}
};

// 
// FLandscapeBrush
//

void FLandscapeBrush::BeginStroke(FLOAT LandscapeX, FLOAT LandscapeY, class FLandscapeTool* CurrentTool)
{
	GEditor->BeginTransaction( *FString::Printf(LocalizeSecure(LocalizeUnrealEd("LandscapeMode_EditTransaction"), CurrentTool->GetIconString())) );
}

void FLandscapeBrush::EndStroke()
{
	GEditor->EndTransaction();
}

// 
// FLandscapeBrushCircle
//

class FLandscapeBrushCircle : public FLandscapeBrush
{
	ALandscape* LastLandscape;

	TSet<ULandscapeComponent*> BrushMaterialComponents;
protected:
	FVector2D LastMousePosition;
	UMaterialInstanceConstant* BrushMaterial;
	virtual FLOAT CalculateFalloff( FLOAT Distance, FLOAT Radius, FLOAT Falloff ) = 0;
public:
	class FEdModeLandscape* EdMode;

	FLandscapeBrushCircle(class FEdModeLandscape* InEdMode)
	:	EdMode(InEdMode)
	{
		BrushMaterial = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass());
		BrushMaterial->AddToRoot();
	}

	virtual ~FLandscapeBrushCircle()
	{
		BrushMaterial->RemoveFromRoot();
	}

	void LeaveBrush()
	{
		for( TSet<ULandscapeComponent*>::TIterator It(BrushMaterialComponents); It; ++It )
		{
			if( (*It)->EditToolRenderData != NULL )
			{
				(*It)->EditToolRenderData->Update(NULL);
			}
		}
		BrushMaterialComponents.Empty();
	}

	void BeginStroke(FLOAT LandscapeX, FLOAT LandscapeY, class FLandscapeTool* CurrentTool)
	{
		FLandscapeBrush::BeginStroke(LandscapeX,LandscapeY,CurrentTool);
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
	{
		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;
		if( Landscape )
		{
			FLOAT ScaleXY = Landscape->DrawScale3D.X * Landscape->DrawScale;
			FLOAT Radius = (1.f - EdMode->UISettings.GetBrushFalloff()) * EdMode->UISettings.GetBrushRadius() / ScaleXY;
			FLOAT Falloff = EdMode->UISettings.GetBrushFalloff() * EdMode->UISettings.GetBrushRadius() / ScaleXY;

			// Set params for brush material.
			FVector WorldLocation = FVector(LastMousePosition.X,LastMousePosition.Y,0) * ScaleXY + Landscape->Location;
			BrushMaterial->SetScalarParameterValue(FName(TEXT("WorldRadius")), ScaleXY*Radius);
			BrushMaterial->SetScalarParameterValue(FName(TEXT("WorldFalloff")), ScaleXY*Falloff);
			BrushMaterial->SetVectorParameterValue(FName(TEXT("WorldPosition")), FLinearColor(WorldLocation.X,WorldLocation.Y,WorldLocation.Z,ScaleXY));

			// Set brush material.
			INT X1 = appFloor(LastMousePosition.X - (Radius+Falloff));
			INT Y1 = appFloor(LastMousePosition.Y - (Radius+Falloff));
			INT X2 = appCeil(LastMousePosition.X + (Radius+Falloff));
			INT Y2 = appCeil(LastMousePosition.Y + (Radius+Falloff));

			TSet<ULandscapeComponent*> NewComponents;
			Landscape->GetComponentsInRegion(X1,Y1,X2,Y2,NewComponents);

			// Set brush material for components in new region
			for( TSet<ULandscapeComponent*>::TIterator It(NewComponents); It; ++It )
			{
				if( (*It)->EditToolRenderData != NULL )
				{
					(*It)->EditToolRenderData->Update(BrushMaterial);
				}
			}

			// Remove the material from any old components that are no longer in the region
			TSet<ULandscapeComponent*> RemovedComponents = BrushMaterialComponents.Difference(NewComponents);
			for ( TSet<ULandscapeComponent*>::TIterator It(RemovedComponents); It; ++It )
			{
				if( (*It)->EditToolRenderData != NULL )
				{
					(*It)->EditToolRenderData->Update(NULL);
				}
			}

			BrushMaterialComponents = NewComponents;	
		}
	}

	void MouseMove(FLOAT LandscapeX, FLOAT LandscapeY)
	{
		FVector2D NewMousePosition = FVector2D(LandscapeX, LandscapeY);
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	UBOOL InputKey( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FName InKey, EInputEvent InEvent )
	{
		UBOOL bUpdate = FALSE;
#if 0
		if( InEvent == IE_Pressed || IE_Repeat )
		{
			if( InKey == KEY_LeftBracket )
			{
				if( IsShiftDown(InViewport) )
				{
					Falloff = Max<FLOAT>(Falloff - 0.5f, 0.f);
				}
				else
				{
					Radius = Max<FLOAT>(Radius - 0.5f, 0.f);
				}
				bUpdate = TRUE;
			}
			if( InKey == KEY_RightBracket )
			{
				if( IsShiftDown(InViewport) )
				{
					Falloff += 0.5f;
				}
				else
				{
					Radius += 0.5f;
				}
				bUpdate = TRUE;
			}
		}

		if( bUpdate )
		{
			ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;
			FLOAT ScaleXY = Landscape->DrawScale3D.X * Landscape->DrawScale;
			BrushMaterial->SetScalarParameterValue(FName(TEXT("WorldRadius")), ScaleXY*Radius);
			BrushMaterial->SetScalarParameterValue(FName(TEXT("WorldFalloff")), ScaleXY*Falloff);
		}
#endif

		return bUpdate;
	}

	FLOAT GetBrushExtent()
	{
		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;
		FLOAT ScaleXY = Landscape->DrawScale3D.X * Landscape->DrawScale;

		return 2.f * EdMode->UISettings.GetBrushRadius() / ScaleXY;
	}

	void ApplyBrush( TMap<QWORD, FLOAT>& OutBrush, INT& X1, INT& Y1, INT& X2, INT& Y2 )
	{
		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;
		FLOAT ScaleXY = Landscape->DrawScale3D.X * Landscape->DrawScale;
		
		FLOAT Radius = (1.f - EdMode->UISettings.GetBrushFalloff()) * EdMode->UISettings.GetBrushRadius() / ScaleXY;
		FLOAT Falloff = EdMode->UISettings.GetBrushFalloff() * EdMode->UISettings.GetBrushRadius() / ScaleXY;

		X1 = appFloor(LastMousePosition.X - (Radius+Falloff));
		Y1 = appFloor(LastMousePosition.Y - (Radius+Falloff));
		X2 = appCeil(LastMousePosition.X + (Radius+Falloff));
		Y2 = appCeil(LastMousePosition.Y + (Radius+Falloff));

		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				QWORD VertexKey = ALandscape::MakeKey(X,Y);

				// Distance from mouse
				FLOAT MouseDist = appSqrt(Square(LastMousePosition.X-(FLOAT)X) + Square(LastMousePosition.Y-(FLOAT)Y));

				FLOAT PaintAmount = CalculateFalloff(MouseDist, Radius, Falloff);

				if( PaintAmount > 0.f )
				{
					// Set the brush value for this vertex
					OutBrush.Set(VertexKey, PaintAmount);
				}
			}
		}
	}
};

// 
// FLandscapeBrushSelection
//

class FLandscapeBrushSelection : public FLandscapeBrush
{
	ALandscape* LastLandscape;

	TSet<ULandscapeComponent*> BrushMaterialComponents;

	const TCHAR* GetIconString() { return TEXT("Selection"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Selection"); };
protected:
	FVector2D LastMousePosition;
	UMaterial* BrushMaterial;
public:
	class FEdModeLandscape* EdMode;

	FLandscapeBrushSelection(class FEdModeLandscape* InEdMode)
		:	EdMode(InEdMode),
		BrushMaterial(NULL)
	{
		BrushMaterial = LoadObject<UMaterial>(NULL, TEXT("EditorLandscapeResources.SelectBrushMaterial"), NULL, LOAD_None, NULL);
		if (BrushMaterial)
		{
			BrushMaterial->AddToRoot();
		}
	}

	virtual ~FLandscapeBrushSelection()
	{
		if (BrushMaterial)
		{
			BrushMaterial->RemoveFromRoot();
		}
	}

	void LeaveBrush()
	{
		for( TSet<ULandscapeComponent*>::TIterator It(BrushMaterialComponents); It; ++It )
		{
			if( (*It)->EditToolRenderData != NULL )
			{
				(*It)->EditToolRenderData->Update(NULL);
			}
		}
		BrushMaterialComponents.Empty();

		for( TSet<ULandscapeComponent*>::TIterator It(ALandscape::SelectedComponents); It; ++It )
		{
			if( (*It)->EditToolRenderData != NULL )
			{
				(*It)->EditToolRenderData->UpdateSelectionMaterial(FALSE);
			}
		}
		ALandscape::SelectedComponents.Empty();	
		ALandscape::SelectedCollisionComponents.Empty();
	}

	void BeginStroke(FLOAT LandscapeX, FLOAT LandscapeY, class FLandscapeTool* CurrentTool)
	{
		FLandscapeBrush::BeginStroke(LandscapeX,LandscapeY,CurrentTool);
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
	{
		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;

		if( Landscape )
		{
			FLOAT ScaleXY = Landscape->DrawScale3D.X * Landscape->DrawScale;
			FLOAT Radius = EdMode->UISettings.GetBrushRadius() / ScaleXY;
			INT X1 = appFloor(LastMousePosition.X - Radius);
			INT Y1 = appFloor(LastMousePosition.Y - Radius);
			INT X2 = appCeil(LastMousePosition.X + Radius);
			INT Y2 = appCeil(LastMousePosition.Y + Radius);

			// Only select one component for now
			INT X = appRound(LastMousePosition.X);
			INT Y = appRound(LastMousePosition.Y);

			TSet<ULandscapeComponent*> NewComponents;
			//Landscape->GetComponentsInRegion(X1,Y1,X2,Y2,NewComponents);
			INT ComponentIndexX = (X >= 0.f) ? X / Landscape->ComponentSizeQuads : (X+1) / Landscape->ComponentSizeQuads - 1;
			INT ComponentIndexY = (Y >= 0.f) ? Y / Landscape->ComponentSizeQuads : (Y+1) / Landscape->ComponentSizeQuads - 1;
			ULandscapeComponent* Component = Landscape->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*Landscape->ComponentSizeQuads,ComponentIndexY*Landscape->ComponentSizeQuads));
			if (Component)
			{
				NewComponents.Add(Component);
			}

			// Set brush material for components in new region
			for( TSet<ULandscapeComponent*>::TIterator It(NewComponents); It; ++It )
			{
				if( (*It)->EditToolRenderData != NULL )
				{
					(*It)->EditToolRenderData->Update(BrushMaterial);
				}
			}

			// Remove the material from any old components that are no longer in the region
			TSet<ULandscapeComponent*> RemovedComponents = BrushMaterialComponents.Difference(NewComponents);
			for ( TSet<ULandscapeComponent*>::TIterator It(RemovedComponents); It; ++It )
			{
				if( (*It)->EditToolRenderData != NULL )
				{
					(*It)->EditToolRenderData->Update(NULL);
				}
			}

			BrushMaterialComponents = NewComponents;	
		}
	}

	void MouseMove(FLOAT LandscapeX, FLOAT LandscapeY)
	{
		FVector2D NewMousePosition = FVector2D(LandscapeX, LandscapeY);
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	UBOOL InputKey( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FName InKey, EInputEvent InEvent )
	{
		UBOOL bUpdate = FALSE;
		

		return bUpdate;
	}

	FLOAT GetBrushExtent()
	{
		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;
		FLOAT ScaleXY = Landscape->DrawScale3D.X * Landscape->DrawScale;

		return 2.f * EdMode->UISettings.GetBrushRadius() / ScaleXY;
	}

	void ApplyBrush( TMap<QWORD, FLOAT>& OutBrush, INT& X1, INT& Y1, INT& X2, INT& Y2 )
	{
		// Selection Brush only works for 
		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;
		FLOAT ScaleXY = Landscape->DrawScale3D.X * Landscape->DrawScale;

		FLOAT Radius = EdMode->UISettings.GetBrushRadius() / ScaleXY;

		X1= INT_MAX;
		Y1= INT_MAX;
		X2= INT_MIN;
		Y2= INT_MIN;

		// Get extent for all components
		for ( TSet<ULandscapeComponent*>::TIterator It(BrushMaterialComponents); It; ++It )
		{
			if( *It )
			{
				if( (*It)->SectionBaseX < X1 )
				{
					X1 = (*It)->SectionBaseX;
				}
				if( (*It)->SectionBaseY < Y1 )
				{
					Y1 = (*It)->SectionBaseY;
				}
				if( (*It)->SectionBaseX+(*It)->ComponentSizeQuads > X2 )
				{
					X2 = (*It)->SectionBaseX+(*It)->ComponentSizeQuads;
				}
				if( (*It)->SectionBaseY+(*It)->ComponentSizeQuads > Y2 )
				{
					Y2 = (*It)->SectionBaseY+(*It)->ComponentSizeQuads;
				}
			}
		}

		// Check for add component...
		if (GLandscapeRenderAddCollision)
		{
			INT X = appRound(LastMousePosition.X);
			INT Y = appRound(LastMousePosition.Y);

			INT ComponentX = (X >= 0) ? X / Landscape->ComponentSizeQuads : (X+1) / Landscape->ComponentSizeQuads - 1;
			INT ComponentY = (Y >= 0) ? Y / Landscape->ComponentSizeQuads : (Y+1) / Landscape->ComponentSizeQuads - 1;

			X1 = ComponentX * Landscape->ComponentSizeQuads;
			Y1 = ComponentY * Landscape->ComponentSizeQuads;
			X2 = X1 + Landscape->ComponentSizeQuads;
			Y2 = Y1 + Landscape->ComponentSizeQuads;
		}

		// Should not be possible...
		check(X1 <= X2 && Y1 <= Y2);

		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				QWORD VertexKey = ALandscape::MakeKey(X,Y);

				// Set the brush value for this vertex
				OutBrush.Set(VertexKey, 1.0f);
			}
		}
	}
};

class FLandscapeBrushCircle_Linear : public FLandscapeBrushCircle
{
public:
	FLandscapeBrushCircle_Linear(class FEdModeLandscape* InEdMode)
	:	FLandscapeBrushCircle(InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Linear = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.CircleBrushMaterial_Linear"), NULL, LOAD_None, NULL);
		BrushMaterial->SetParent(CircleBrushMaterial_Linear);
	}


	const TCHAR* GetIconString() { return TEXT("Circle_linear"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Falloff_Linear"); };

protected:
	virtual FLOAT CalculateFalloff( FLOAT Distance, FLOAT Radius, FLOAT Falloff )
	{
		return Distance < Radius ? 1.f : 
			Falloff > 0.f ? Max<FLOAT>(0.f, 1.f - (Distance - Radius) / Falloff) : 
			0.f;
	}
};

class FLandscapeBrushCircle_Smooth : public FLandscapeBrushCircle_Linear
{
public:
	FLandscapeBrushCircle_Smooth(class FEdModeLandscape* InEdMode)
	:	FLandscapeBrushCircle_Linear(InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Smooth = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.CircleBrushMaterial_Smooth"), NULL, LOAD_None, NULL);
		BrushMaterial->SetParent(CircleBrushMaterial_Smooth);
	}

	const TCHAR* GetIconString() { return TEXT("Circle_smooth"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Falloff_Smooth"); };

protected:
	virtual FLOAT CalculateFalloff( FLOAT Distance, FLOAT Radius, FLOAT Falloff )
	{
		FLOAT y = FLandscapeBrushCircle_Linear::CalculateFalloff(Distance, Radius, Falloff);
		// Smooth-step it
		return y*y*(3-2*y);
	}
};

class FLandscapeBrushCircle_Spherical : public FLandscapeBrushCircle
{
public:
	FLandscapeBrushCircle_Spherical(class FEdModeLandscape* InEdMode)
	:	FLandscapeBrushCircle(InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Spherical = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.CircleBrushMaterial_Spherical"), NULL, LOAD_None, NULL);
		BrushMaterial->SetParent(CircleBrushMaterial_Spherical);
	}

	const TCHAR* GetIconString() { return TEXT("Circle_spherical"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Falloff_Spherical"); };

protected:
	virtual FLOAT CalculateFalloff( FLOAT Distance, FLOAT Radius, FLOAT Falloff )
	{
		if( Distance <= Radius )
		{
			return 1.f;
		}

		if( Distance > Radius + Falloff )
		{
			return 0.f;
		}

		// Elliptical falloff
		return appSqrt( 1.f - Square((Distance - Radius) / Falloff) );
	}
};

class FLandscapeBrushCircle_Tip : public FLandscapeBrushCircle
{
public:
	FLandscapeBrushCircle_Tip(class FEdModeLandscape* InEdMode)
	:	FLandscapeBrushCircle(InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Tip = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.CircleBrushMaterial_Tip"), NULL, LOAD_None, NULL);
		BrushMaterial->SetParent(CircleBrushMaterial_Tip);
	}

	const TCHAR* GetIconString() { return TEXT("Circle_tip"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_Falloff_Tip"); };

protected:
	virtual FLOAT CalculateFalloff( FLOAT Distance, FLOAT Radius, FLOAT Falloff )
	{
		if( Distance <= Radius )
		{
			return 1.f;
		}

		if( Distance > Radius + Falloff )
		{
			return 0.f;
		}

		// inverse elliptical falloff
		return 1.f - appSqrt( 1.f - Square((Falloff + Radius - Distance) / Falloff) );
	}
};

// FLandscapeBrushAlphaPattern
class FLandscapeBrushAlphaPattern : public FLandscapeBrushCircle_Smooth
{

public:
	FLandscapeBrushAlphaPattern(class FEdModeLandscape* InEdMode)
	:	FLandscapeBrushCircle_Smooth(InEdMode)
	{
		UMaterialInstanceConstant* AlphaBrushMaterial = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.AlphaBrushMaterial_Smooth"), NULL, LOAD_None, NULL);
		BrushMaterial->SetParent(AlphaBrushMaterial);
	}

	void ApplyBrush( TMap<QWORD, FLOAT>& OutBrush, INT& X1, INT& Y1, INT& X2, INT& Y2 )
	{
		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;
		FLOAT ScaleXY = Landscape->DrawScale3D.X * Landscape->DrawScale;

		FLOAT Radius = (1.f - EdMode->UISettings.GetBrushFalloff()) * EdMode->UISettings.GetBrushRadius() / ScaleXY;
		FLOAT Falloff = EdMode->UISettings.GetBrushFalloff() * EdMode->UISettings.GetBrushRadius() / ScaleXY;

		INT SizeX = EdMode->UISettings.GetAlphaTextureSizeX();
		INT SizeY = EdMode->UISettings.GetAlphaTextureSizeY();
		const BYTE* AlphaData = EdMode->UISettings.GetAlphaTextureData();

		X1 = appFloor(LastMousePosition.X - (Radius+Falloff));
		Y1 = appFloor(LastMousePosition.Y - (Radius+Falloff));
		X2 = appCeil(LastMousePosition.X + (Radius+Falloff));
		Y2 = appCeil(LastMousePosition.Y + (Radius+Falloff));

		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				QWORD VertexKey = ALandscape::MakeKey(X,Y);

				// Find alphamap sample location
				FLOAT SampleX = (FLOAT)X / EdMode->UISettings.GetAlphaBrushScale() + (FLOAT)SizeX * EdMode->UISettings.GetAlphaBrushPanU();
				FLOAT SampleY = (FLOAT)Y / EdMode->UISettings.GetAlphaBrushScale() + (FLOAT)SizeY * EdMode->UISettings.GetAlphaBrushPanV();

				FLOAT Angle = PI * EdMode->UISettings.GetAlphaBrushRotation() / 180.f;

				FLOAT ModSampleX = appFmod( SampleX * appCos(Angle) - SampleY * appSin(Angle), (FLOAT)SizeX );
				FLOAT ModSampleY = appFmod( SampleY * appCos(Angle) + SampleX * appSin(Angle), (FLOAT)SizeY );

				if( ModSampleX < 0.f )
				{
					ModSampleX += (FLOAT)SizeX;
				}
				if( ModSampleY < 0.f )
				{
					ModSampleY += (FLOAT)SizeY;
				}

				// Bilinear interpolate the values from the alpha texture
				INT SampleX0 = appFloor(ModSampleX);
				INT SampleX1 = (SampleX0+1) % SizeX;
				INT SampleY0 = appFloor(ModSampleY);
				INT SampleY1 = (SampleY0+1) % SizeY;

				FLOAT Alpha00 = (FLOAT)AlphaData[ SampleX0 + SampleY0 * SizeX ] / 255.f;
				FLOAT Alpha01 = (FLOAT)AlphaData[ SampleX0 + SampleY1 * SizeX ] / 255.f;
				FLOAT Alpha10 = (FLOAT)AlphaData[ SampleX1 + SampleY0 * SizeX ] / 255.f;
				FLOAT Alpha11 = (FLOAT)AlphaData[ SampleX1 + SampleY1 * SizeX ] / 255.f;

				FLOAT Alpha = Lerp(
									Lerp( Alpha00, Alpha01, appFractional(SampleX) ),
									Lerp( Alpha10, Alpha11, appFractional(SampleX) ),
									appFractional(SampleY)
								);

				// Distance from mouse
				FLOAT MouseDist = appSqrt(Square(LastMousePosition.X-(FLOAT)X) + Square(LastMousePosition.Y-(FLOAT)Y));

				FLOAT PaintAmount = CalculateFalloff(MouseDist, Radius, Falloff) * Alpha;

				if( PaintAmount > 0.f )
				{
					// Set the brush value for this vertex
					OutBrush.Set(VertexKey, PaintAmount);
				}
			}
		}
	}

	void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
	{
		FLandscapeBrushCircle::Tick(ViewportClient,DeltaTime);

		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;
		if( Landscape )
		{
			FLOAT ScaleXY = Landscape->DrawScale3D.X * Landscape->DrawScale;
			INT SizeX = EdMode->UISettings.GetAlphaTextureSizeX();
			INT SizeY = EdMode->UISettings.GetAlphaTextureSizeY();

			FLinearColor AlphaScaleBias(
				1.f / (EdMode->UISettings.GetAlphaBrushScale() * ScaleXY * SizeX),
				1.f / (EdMode->UISettings.GetAlphaBrushScale() * ScaleXY * SizeY),
				EdMode->UISettings.GetAlphaBrushPanU(),
				EdMode->UISettings.GetAlphaBrushPanV()
				);
			BrushMaterial->SetVectorParameterValue(FName(TEXT("AlphaScaleBias")), AlphaScaleBias);

			FLOAT Angle = PI * EdMode->UISettings.GetAlphaBrushRotation() / 180.f;
			FLinearColor LandscapeLocation(Landscape->Location.X,Landscape->Location.Y,Landscape->Location.Z,Angle);
			BrushMaterial->SetVectorParameterValue(FName(TEXT("LandscapeLocation")), LandscapeLocation);

			INT Channel = EdMode->UISettings.GetAlphaTextureChannel();
			FLinearColor AlphaTextureMask(Channel==0?1:0,Channel==1?1:0,Channel==2?1:0,Channel==3?1:0);
			BrushMaterial->SetVectorParameterValue(FName(TEXT("AlphaTextureMask")), AlphaTextureMask);
			BrushMaterial->SetTextureParameterValue(FName(TEXT("AlphaTexture")), EdMode->UISettings.GetAlphaTexture() );
		}
	}

	const TCHAR* GetIconString() { return TEXT("Pattern"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Brush_PatternAlpha"); };
};

struct FHeightmapToolTarget
{
	typedef FLandscapeHeightCache CacheClass;
	enum EToolTargetType { TargetType = LET_Heightmap };

	static FLOAT StrengthMultiplier(ALandscape* Landscape)
	{
		return 10.f * Landscape->DrawScale * Landscape->DrawScale3D.X / (Landscape->DrawScale3D.Z / 128.f);
	}
};


struct FWeightmapToolTarget
{
	typedef FLandscapeAlphaCache CacheClass;
	enum EToolTargetType { TargetType = LET_Weightmap };

	static FLOAT StrengthMultiplier(ALandscape* Landscape)
	{
		return 255.f;
	}
};

// 
// FLandscapeToolPaintBase
//
template<class ToolTarget>
class FLandscapeToolPaintBase : public FLandscapeTool
{
private:
	FLOAT PrevHitX;
	FLOAT PrevHitY;
protected:
	FLOAT PaintDistance;

public:
	FLandscapeToolPaintBase(FEdModeLandscape* InEdMode)
	:	EdMode(InEdMode)
	,	bToolActive(FALSE)
	,	Landscape(NULL)
	,	Cache(NULL)
	{
	}

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return Target.TargetType == ToolTarget::TargetType;
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		Landscape = InTarget.Landscape;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);
		PaintDistance = 0;
		PrevHitX = InHitX;
		PrevHitY = InHitY;

		Cache = new ToolTarget::CacheClass(InTarget);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		delete Cache;
		Landscape = NULL;
		bToolActive = FALSE;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, HitX, HitY)  )
		{
			PaintDistance += appSqrt(Square(PrevHitX - HitX) + Square(PrevHitY - HitY));
			PrevHitX = HitX;
			PrevHitY = HitY;

			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
			}

			if( bToolActive )
			{
				// Apply tool
				ApplyTool(ViewportClient);
			}
		}

		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool(FEditorLevelViewportClient* ViewportClient) = 0;

protected:
	class FEdModeLandscape* EdMode;
	UBOOL bToolActive;
	class ALandscape* Landscape;

	typename ToolTarget::CacheClass* Cache;
};


// 
// FLandscapeToolPaint
//
template<class ToolTarget>
class FLandscapeToolPaint : public FLandscapeToolPaintBase<ToolTarget>
{
	struct FBrushHistoryInfo
	{
		FLOAT PaintDistance;
		typename ToolTarget::CacheClass::DataType OriginalValue;
		typename ToolTarget::CacheClass::DataType PaintAmount;
		
		FBrushHistoryInfo(FLOAT InPaintDistance, typename ToolTarget::CacheClass::DataType InOriginalValue, typename ToolTarget::CacheClass::DataType InPaintAmount)
		:	PaintDistance(InPaintDistance)
		,	OriginalValue(InOriginalValue)
		,	PaintAmount(InPaintAmount)
		{}
	};
	
	TMap<QWORD, FBrushHistoryInfo> BrushHistory;

public:
	FLandscapeToolPaint(class FEdModeLandscape* InEdMode)
		:	FLandscapeToolPaintBase(InEdMode)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Paint"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Paint"); };

	virtual void ApplyTool(FEditorLevelViewportClient* ViewportClient)
	{
		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2);

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		Cache->CacheData(X1,Y1,X2,Y2);

		TArray<ToolTarget::CacheClass::DataType> Data;
		Cache->GetCachedData(X1,Y1,X2,Y2,Data);

		// Invert when holding Shift
		UBOOL bInvert = IsShiftDown(ViewportClient->Viewport);

		// Brush Size
		FLOAT BrushSizeAdjust = 4.0f;
		if (ToolTarget::TargetType != LET_Weightmap && EdMode->UISettings.GetBrushRadius() < EdMode->UISettings.GetMaximumValueRadius())
		{
			BrushSizeAdjust = 4.0f * EdMode->UISettings.GetBrushRadius() / EdMode->UISettings.GetMaximumValueRadius();
		}

		// Apply the brush	
		for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
		{
			INT X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);

			// Value before we apply our painting
			ToolTarget::CacheClass::DataType OriginalValue = Data((X-X1) + (Y-Y1)*(1+X2-X1));
			ToolTarget::CacheClass::DataType PaintAmount = appRound( It.Value() * EdMode->UISettings.GetToolStrength() * Pressure * ToolTarget::StrengthMultiplier(Landscape));

			if (ToolTarget::TargetType != LET_Weightmap)
			{
				PaintAmount *= BrushSizeAdjust;
			}

			FBrushHistoryInfo* History = BrushHistory.Find(It.Key());
			if( History )
			{
				FLOAT BrushExtent = EdMode->CurrentBrush->GetBrushExtent();
				FLOAT HistoryDistance = PaintDistance - History->PaintDistance;
				if( HistoryDistance < 5.f * BrushExtent )
				{
					FLOAT Alpha = Clamp<FLOAT>( (HistoryDistance - BrushExtent) / (5.f * BrushExtent), 0.f, 1.f);
					OriginalValue = Lerp( History->OriginalValue, OriginalValue, Alpha );
					PaintAmount = Max(PaintAmount, History->PaintAmount);
				}
			}

			if( bInvert )
			{
				PaintAmount = OriginalValue - ToolTarget::CacheClass::ClampValue( OriginalValue - PaintAmount );
				Data((X-X1) + (Y-Y1)*(1+X2-X1)) = OriginalValue - PaintAmount;
			}
			else
			{
				PaintAmount = ToolTarget::CacheClass::ClampValue( OriginalValue + PaintAmount ) - OriginalValue;
				Data((X-X1) + (Y-Y1)*(1+X2-X1)) = OriginalValue + PaintAmount;
			}

			// Save the original and paint amount values we used
			BrushHistory.Set(It.Key(), FBrushHistoryInfo(PaintDistance, OriginalValue, PaintAmount));
		}

		Cache->SetCachedData(X1,Y1,X2,Y2,Data);
		Cache->Flush();
	}

	virtual void EndTool()
	{
		BrushHistory.Empty();
		FLandscapeToolPaintBase::EndTool();
	}

};

#if WITH_KISSFFT
#include "tools/kiss_fftnd.h" // Kiss FFT for Real component...
#endif

template<typename DataType>
inline void LowPassFilter(INT X1, INT Y1, INT X2, INT Y2, TMap<QWORD, FLOAT>& BrushInfo, TArray<DataType>& Data, const FLOAT DetailScale, const FLOAT ApplyRatio = 1.f)
{
#if WITH_KISSFFT
	// Low-pass filter
	INT FFTWidth = X2-X1-1;
	INT FFTHeight = Y2-Y1-1;

	const int NDims = 2;
	const INT Dims[NDims] = {FFTHeight-FFTHeight%2, FFTWidth-FFTWidth%2};
	kiss_fftnd_cfg stf = kiss_fftnd_alloc(Dims, NDims, 0, NULL, NULL),
					sti = kiss_fftnd_alloc(Dims, NDims, 1, NULL, NULL);

	kiss_fft_cpx *buf = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * Dims[0] * Dims[1]);
	kiss_fft_cpx *out = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * Dims[0] * Dims[1]);

	for (int X = X1+1; X <= X2-1-FFTWidth%2; X++)
	{
		for (int Y = Y1+1; Y <= Y2-1-FFTHeight%2; Y++)
		{
			buf[(X-X1-1) + (Y-Y1-1)*(Dims[1])].r = Data((X-X1) + (Y-Y1)*(1+X2-X1));
			buf[(X-X1-1) + (Y-Y1-1)*(Dims[1])].i = 0;
		}
	}

	// Forward FFT
	kiss_fftnd(stf, buf, out);

	INT CenterPos[2] = {Dims[0]>>1, Dims[1]>>1};
	for (int Y = 0; Y < Dims[0]; Y++)
	{
		FLOAT DistFromCenter = 0.f;
		for (int X = 0; X < Dims[1]; X++)
		{
			if (Y < CenterPos[0])
			{
				if (X < CenterPos[1])
				{
					// 1
					DistFromCenter = X*X + Y*Y;
				}
				else
				{
					// 2
					DistFromCenter = (X-Dims[1])*(X-Dims[1]) + Y*Y;
				}
			}
			else
			{
				if (X < CenterPos[1])
				{
					// 3
					DistFromCenter = X*X + (Y-Dims[0])*(Y-Dims[0]);
				}
				else
				{
					// 4
					DistFromCenter = (X-Dims[1])*(X-Dims[1]) + (Y-Dims[0])*(Y-Dims[0]);
				}
			}
			// High frequency removal
			FLOAT Ratio = 1.f - DetailScale;
			FLOAT Dist = Min<FLOAT>((Dims[0]*Ratio)*(Dims[0]*Ratio), (Dims[1]*Ratio)*(Dims[1]*Ratio));
			FLOAT Filter = 1.0 / (1.0 + DistFromCenter/Dist);
			out[X+Y*Dims[1]].r *= Filter;
			out[X+Y*Dims[1]].i *= Filter;
		}
	}

	// Inverse FFT
	kiss_fftnd(sti, out, buf);

	FLOAT Scale = Dims[0] * Dims[1];
	for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
	{
		INT X, Y;
		ALandscape::UnpackKey(It.Key(), X, Y);

		if (It.Value() > 0.f)
		{
			Data((X-X1) + (Y-Y1)*(1+X2-X1)) = Lerp((FLOAT)Data((X-X1) + (Y-Y1)*(1+X2-X1)), buf[(X-X1-1) + (Y-Y1-1)*(Dims[1])].r / Scale, It.Value() * ApplyRatio);
				//buf[(X-X1-1) + (Y-Y1-1)*(Dims[1])].r / Scale;
		}
	}

	// Free FFT allocation
	KISS_FFT_FREE(stf);
	KISS_FFT_FREE(sti);
	KISS_FFT_FREE(buf);
	KISS_FFT_FREE(out);
#endif
}


// 
// FLandscapeToolSmooth
//
template<class ToolTarget>
class FLandscapeToolSmooth : public FLandscapeToolPaintBase<ToolTarget>
{
public:
	FLandscapeToolSmooth(class FEdModeLandscape* InEdMode)
		:	FLandscapeToolPaintBase(InEdMode)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Smooth"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Smooth"); };

	virtual void ApplyTool(FEditorLevelViewportClient* ViewportClient)
	{
		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2);

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		Cache->CacheData(X1,Y1,X2,Y2);

		TArray<ToolTarget::CacheClass::DataType> Data;
		Cache->GetCachedData(X1,Y1,X2,Y2,Data);

		// Apply the brush
		if (EdMode->UISettings.GetbDetailSmooth())
		{
			LowPassFilter<ToolTarget::CacheClass::DataType>(X1, Y1, X2, Y2, BrushInfo, Data, EdMode->UISettings.GetDetailScale(), EdMode->UISettings.GetToolStrength() * Pressure);
		}
		else
		{
			for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
			{
				INT X, Y;
				ALandscape::UnpackKey(It.Key(), X, Y);

				if( It.Value() > 0.f )
				{
					// 3x3 filter
					INT FilterValue = 0;
					for( INT y=Y-1;y<=Y+1;y++ )
					{
						for( INT x=X-1;x<=X+1;x++ )
						{
							FilterValue += Data((x-X1) + (y-Y1)*(1+X2-X1));
						}
					}
					FilterValue /= 9;

					INT HeightDataIndex = (X-X1) + (Y-Y1)*(1+X2-X1);
					Data(HeightDataIndex) = Lerp( Data(HeightDataIndex), (ToolTarget::CacheClass::DataType)FilterValue, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure );
				}	
			}
		}

		Cache->SetCachedData(X1,Y1,X2,Y2,Data);
		Cache->Flush();
	}
};

//
// FLandscapeToolFlatten
//
template<class ToolTarget>
class FLandscapeToolFlatten : public FLandscapeToolPaintBase<ToolTarget>
{
	UBOOL bInitializedFlattenHeight;
	INT FlattenHeightX;
	INT FlattenHeightY;
	typename ToolTarget::CacheClass::DataType FlattenHeight;

public:
	FLandscapeToolFlatten(class FEdModeLandscape* InEdMode)
	:	FLandscapeToolPaintBase(InEdMode)
	,	bInitializedFlattenHeight(FALSE)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Flatten"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Flatten"); };

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bInitializedFlattenHeight = FALSE;
		FlattenHeightX = InHitX;
		FlattenHeightY = InHitY;
		return FLandscapeToolPaintBase::BeginTool(ViewportClient, InTarget, InHitX, InHitY);
	}

	virtual void EndTool()
	{
		bInitializedFlattenHeight = FALSE;
		FLandscapeToolPaintBase::EndTool();
	}

	virtual void ApplyTool(FEditorLevelViewportClient* ViewportClient)
	{
		if( !bInitializedFlattenHeight && ToolTarget::TargetType != LET_Weightmap)
		{
			Cache->CacheData(FlattenHeightX,FlattenHeightY,FlattenHeightX,FlattenHeightY);
			ToolTarget::CacheClass::DataType* FlattenHeightPtr = Cache->GetValueRef(FlattenHeightX,FlattenHeightY);
			check(FlattenHeightPtr);
			FlattenHeight = *FlattenHeightPtr;
			bInitializedFlattenHeight = TRUE;
		}

		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2);

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		Cache->CacheData(X1,Y1,X2,Y2);

		TArray<ToolTarget::CacheClass::DataType> HeightData;
		Cache->GetCachedData(X1,Y1,X2,Y2,HeightData);

		// For Add or Sub Flatten Mode
		// Apply Ratio...
		TMap<INT, FLOAT> RatioInfo;
		INT MaxDelta = INT_MIN;
		INT MinDelta = INT_MAX;

		// Apply the brush
		for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
		{
			INT X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);

			if( It.Value() > 0.f )
			{
				INT HeightDataIndex = (X-X1) + (Y-Y1)*(1+X2-X1);
				if( ToolTarget::TargetType == LET_Weightmap )
				{
					// Weightmap painting always flattens to solid color, so the strength slider specifies the flatten amount.
					HeightData(HeightDataIndex) = 255.f * EdMode->UISettings.GetToolStrength() * Pressure * It.Value();
				}
				else
				{
					INT Delta = HeightData(HeightDataIndex) - FlattenHeight;
					switch(EdMode->UISettings.GetFlattenMode())
					{
						case ELandscapeToolNoiseMode::Add:
							if (Delta < 0)
							{
								MinDelta = Min<INT>(Delta, MinDelta);
								RatioInfo.Set(HeightDataIndex, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure * Delta);
							}
							break;
						case ELandscapeToolNoiseMode::Sub:
							if (Delta > 0)
							{
								MaxDelta = Max<INT>(Delta, MaxDelta);
								RatioInfo.Set(HeightDataIndex, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure * Delta);
							}
							break;
						default:
						case ELandscapeToolNoiseMode::Both:
					HeightData(HeightDataIndex) = Lerp( HeightData(HeightDataIndex), FlattenHeight, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure );
							break;
				}
			}	
		}
		}

		for( TMap<INT, FLOAT>::TIterator It(RatioInfo); It; ++It )
		{
			switch(EdMode->UISettings.GetFlattenMode())
			{
			case ELandscapeToolNoiseMode::Add:
				HeightData(It.Key()) = Lerp( HeightData(It.Key()), FlattenHeight, It.Value() / (FLOAT)MinDelta );
				break;
			case ELandscapeToolNoiseMode::Sub:
				HeightData(It.Key()) = Lerp( HeightData(It.Key()), FlattenHeight, It.Value() / (FLOAT)MaxDelta );
				break;
			default:
				break;
			}
		}

		Cache->SetCachedData(X1,Y1,X2,Y2,HeightData);
		Cache->Flush();
	}
};

//
// FLandscapeToolErosion
//
class FLandscapeToolErosion : public FLandscapeTool
{
public:
	FLandscapeToolErosion(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	Landscape(NULL)
		,	HeightCache(NULL)
		,	WeightCache(NULL)
		,	bWeightApplied(FALSE)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Erosion"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Erosion"); };

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return TRUE; // erosion applied to all...
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		Landscape = InTarget.Landscape;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);

		HeightCache = new FLandscapeHeightCache(InTarget);
		WeightCache = new FLandscapeFullWeightCache(InTarget);

		bWeightApplied = InTarget.TargetType != LET_Heightmap;

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		delete HeightCache;
		delete WeightCache;
		Landscape = NULL;
		bToolActive = FALSE;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, HitX, HitY)  )
		{
			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
			}

			if( bToolActive )
			{
				// Apply tool
				ApplyTool(ViewportClient);
			}
		}

		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2);

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		const INT NeighborNum = 4;
		const INT Iteration = EdMode->UISettings.GetErodeIterationNum();
		const INT Thickness = EdMode->UISettings.GetErodeSurfaceThickness();
		const INT LayerNum = Landscape->LayerInfos.Num();

		HeightCache->CacheData(X1,Y1,X2,Y2);
		TArray<WORD> HeightData;
		HeightCache->GetCachedData(X1,Y1,X2,Y2,HeightData);

		TArray<BYTE> WeightDatas; // Weight*Layers...
		WeightCache->CacheData(X1,Y1,X2,Y2);
		WeightCache->GetCachedData(X1,Y1,X2,Y2, WeightDatas, LayerNum);	

		// Invert when holding Shift
		UBOOL bInvert = IsShiftDown(ViewportClient->Viewport);

		// Apply the brush	
		WORD Thresh = EdMode->UISettings.GetErodeThresh();
		INT WeightMoveThresh = Min<INT>(Max<INT>(Thickness >> 2, Thresh), Thickness >> 1);

		DWORD SlopeTotal;
		WORD SlopeMax;
		FLOAT TotalHeightDiff;
		FLOAT TotalWeight;

		TArray<FLOAT> CenterWeight;
		CenterWeight.Empty(LayerNum);
		CenterWeight.Add(LayerNum);
		TArray<FLOAT> NeighborWeight;
		NeighborWeight.Empty(NeighborNum*LayerNum);
		NeighborWeight.Add(NeighborNum*LayerNum);

		UBOOL bHasChanged = FALSE;
		for (INT i = 0; i < Iteration; i++)
		{
			bHasChanged = FALSE;
			for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
			{
				INT X, Y;
				ALandscape::UnpackKey(It.Key(), X, Y);

				if( It.Value() > 0.f )
				{
					INT Center = (X-X1) + (Y-Y1)*(1+X2-X1);
					INT Neighbor[NeighborNum] = {(X-1-X1) + (Y-Y1)*(1+X2-X1), (X+1-X1) + (Y-Y1)*(1+X2-X1), (X-X1) + (Y-1-Y1)*(1+X2-X1), (X-X1) + (Y+1-Y1)*(1+X2-X1)};
/*
					INT Neighbor[NeighborNum] = {
													(X-1-X1) + (Y-Y1)*(1+X2-X1), (X+1-X1) + (Y-Y1)*(1+X2-X1), (X-X1) + (Y-1-Y1)*(1+X2-X1), (X-X1) + (Y+1-Y1)*(1+X2-X1)
													,(X-1-X1) + (Y-1-Y1)*(1+X2-X1), (X+1-X1) + (Y+1-Y1)*(1+X2-X1), (X+1-X1) + (Y-1-Y1)*(1+X2-X1), (X-1-X1) + (Y+1-Y1)*(1+X2-X1)
												};
*/
					SlopeTotal = 0;
					SlopeMax = bInvert ? 0 : Thresh;

					for (INT Idx = 0; Idx < NeighborNum; Idx++)
					{
						if (HeightData(Center) > HeightData(Neighbor[Idx]))
						{
							WORD Slope = HeightData(Center) - HeightData(Neighbor[Idx]);
							if (bInvert ^ (Slope*It.Value() > Thresh))
							{
								SlopeTotal += Slope;
								if (SlopeMax < Slope)
								{
									SlopeMax = Slope;
								}
							}
						}
					}

					if (SlopeTotal > 0)
					{
						FLOAT Softness = 1.f;
						for (INT Idx = 0; Idx < LayerNum; Idx++)
						{
							BYTE Weight = WeightDatas(Center*LayerNum + Idx);
							Softness -= (FLOAT)(Weight) / 255.f * Landscape->LayerInfos(Idx).Hardness;
						}
						if (Softness > 0.f)
						{
							//Softness = Clamp<FLOAT>(Softness, 0.f, 1.f);
							TotalHeightDiff = 0;
							INT WeightTransfer = Min<INT>(WeightMoveThresh, (bInvert ? (Thresh - SlopeMax) : (SlopeMax - Thresh)));
							for (INT Idx = 0; Idx < NeighborNum; Idx++)
							{
								TotalWeight = 0.f;
								if (HeightData(Center) > HeightData(Neighbor[Idx]))
								{
									WORD Slope = HeightData(Center) - HeightData(Neighbor[Idx]);
									if (bInvert ^ (Slope > Thresh))
									{
										FLOAT WeightDiff = Softness * EdMode->UISettings.GetToolStrength() * Pressure * ((FLOAT)Slope / SlopeTotal) * It.Value();
										//WORD HeightDiff = (WORD)((SlopeMax - Thresh) * WeightDiff);
										FLOAT HeightDiff = ((bInvert ? (Thresh - SlopeMax) : (SlopeMax - Thresh)) * WeightDiff);
										HeightData(Neighbor[Idx]) += HeightDiff;
										TotalHeightDiff += HeightDiff;

										if (bWeightApplied)
										{
											for (INT LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
											{
												FLOAT CenterWeight = (FLOAT)(WeightDatas(Center*LayerNum + LayerIdx)) / 255.f;
												FLOAT Weight = (FLOAT)(WeightDatas(Neighbor[Idx]*LayerNum + LayerIdx)) / 255.f;
												NeighborWeight(Idx*LayerNum + LayerIdx) = Weight*(FLOAT)Thickness + CenterWeight*WeightDiff*WeightTransfer; // transferred + original...
												TotalWeight += NeighborWeight(Idx*LayerNum + LayerIdx);
											}
											// Need to normalize weight...
											for (INT LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
											{
												WeightDatas(Neighbor[Idx]*LayerNum + LayerIdx) = (BYTE)(255.f * NeighborWeight(Idx*LayerNum + LayerIdx) / TotalWeight);
											}
										}
									}
								}
							}

							HeightData(Center) -= TotalHeightDiff;

							if (bWeightApplied)
							{
								TotalWeight = 0.f;
								FLOAT WeightDiff = Softness * EdMode->UISettings.GetToolStrength() * Pressure * It.Value();

								for (INT LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
								{
									FLOAT Weight = (FLOAT)(WeightDatas(Center*LayerNum + LayerIdx)) / 255.f;
									CenterWeight(LayerIdx) = Weight*Thickness - Weight*WeightDiff*WeightTransfer;
									TotalWeight += CenterWeight(LayerIdx);
								}
								// Need to normalize weight...
								for (INT LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
								{
									WeightDatas(Center*LayerNum + LayerIdx) = (BYTE)(255.f * CenterWeight(LayerIdx) / TotalWeight);
								}
							}

							bHasChanged = TRUE;
						} // if Softness > 0.f
					} // if SlopeTotal > 0
				}
			}
			if (!bHasChanged)
			{
				break;
			}
		}

		FLOAT BrushSizeAdjust = 1.0f;
		if (EdMode->UISettings.GetBrushRadius() < EdMode->UISettings.GetMaximumValueRadius())
		{
			BrushSizeAdjust = EdMode->UISettings.GetBrushRadius() / EdMode->UISettings.GetMaximumValueRadius() / 10.f;
		}

		// Make some noise...
		for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
		{
			INT X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);

			if( It.Value() > 0.f )
			{
				FNoiseParameter NoiseParam(0, EdMode->UISettings.GetErosionNoiseScale(), It.Value() * Thresh * EdMode->UISettings.GetToolStrength() * BrushSizeAdjust);
				FLOAT PaintAmount = ELandscapeToolNoiseMode::Conversion(EdMode->UISettings.GetErosionNoiseMode(), NoiseParam.NoiseAmount, NoiseParam.Sample(X, Y));
				HeightData((X-X1) + (Y-Y1)*(1+X2-X1)) += PaintAmount;
			}
		}

		HeightCache->SetCachedData(X1,Y1,X2,Y2,HeightData);
		HeightCache->Flush();
		if (bWeightApplied)
		{
			WeightCache->SetCachedData(X1,Y1,X2,Y2,WeightDatas, LayerNum);
			WeightCache->Flush();
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ALandscape* Landscape;

	FLandscapeHeightCache* HeightCache;
	FLandscapeFullWeightCache* WeightCache;

	UBOOL bToolActive;
	UBOOL bWeightApplied;
};

//
// FLandscapeToolHydraErosion
//
class FLandscapeToolHydraErosion : public FLandscapeToolErosion
{
public:
	FLandscapeToolHydraErosion(class FEdModeLandscape* InEdMode)
		: FLandscapeToolErosion(InEdMode)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("HydraulicErosion"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_HydraErosion"); };

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2);

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		const INT NeighborNum = 8;
		const INT LayerNum = Landscape->LayerInfos.Num();

		const INT Iteration = EdMode->UISettings.GetHErodeIterationNum();
		const WORD RainAmount = EdMode->UISettings.GetRainAmount();
		const FLOAT DissolvingRatio = 0.07 * EdMode->UISettings.GetToolStrength() * Pressure;  //0.01;
		const FLOAT EvaporateRatio = 0.5;
		const FLOAT SedimentCapacity = 0.10 * EdMode->UISettings.GetSedimentCapacity(); //DissolvingRatio; //0.01;

		HeightCache->CacheData(X1,Y1,X2,Y2);
		TArray<WORD> HeightData;
		HeightCache->GetCachedData(X1,Y1,X2,Y2,HeightData);
/*
		TArray<BYTE> WeightDatas; // Weight*Layers...
		WeightCache->CacheData(X1,Y1,X2,Y2);
		WeightCache->GetCachedData(X1,Y1,X2,Y2, WeightDatas, LayerNum);	
*/
		// Invert when holding Shift
		UBOOL bInvert = IsShiftDown(ViewportClient->Viewport);

		// Apply the brush
		TArray<WORD> WaterData;
		WaterData.Empty((1+X2-X1)*(1+Y2-Y1));
		WaterData.AddZeroed((1+X2-X1)*(1+Y2-Y1));
		TArray<WORD> SedimentData;
		SedimentData.Empty((1+X2-X1)*(1+Y2-Y1));
		SedimentData.AddZeroed((1+X2-X1)*(1+Y2-Y1));

		UBOOL bWaterExist = TRUE;
		UINT TotalHeightDiff;
		UINT TotalAltitudeDiff;
		UINT AltitudeDiff[NeighborNum];
		UINT TotalWaterDiff;
		UINT WaterDiff[NeighborNum];
		UINT TotalSedimentDiff;
		FLOAT AverageAltitude;

		// It's raining men!
		// Only initial raining works better...
		FNoiseParameter NoiseParam(0, EdMode->UISettings.GetRainDistScale(), RainAmount);
		for (int X = X1; X < X2; X++)
		{
			for (int Y = Y1; Y < Y2; Y++)
			{
				FLOAT PaintAmount = ELandscapeToolNoiseMode::Conversion(EdMode->UISettings.GetRainDistMode(), NoiseParam.NoiseAmount, NoiseParam.Sample(X, Y));
				if (PaintAmount > 0) // Raining only for positive region...
					WaterData((X-X1) + (Y-Y1)*(1+X2-X1)) += PaintAmount;
			}
		}

		for (INT i = 0; i < Iteration; i++)
		{
			bWaterExist = FALSE;
			for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
			{
				INT X, Y;
				ALandscape::UnpackKey(It.Key(), X, Y);

				if( It.Value() > 0.f)
				{
					INT Center = (X-X1) + (Y-Y1)*(1+X2-X1);
					//INT Neighbor[NeighborNum] = {(X-1-X1) + (Y-Y1)*(1+X2-X1), (X+1-X1) + (Y-Y1)*(1+X2-X1), (X-X1) + (Y-1-Y1)*(1+X2-X1), (X-X1) + (Y+1-Y1)*(1+X2-X1)};

					INT Neighbor[NeighborNum] = {
						(X-1-X1) + (Y-Y1)*(1+X2-X1), (X+1-X1) + (Y-Y1)*(1+X2-X1), (X-X1) + (Y-1-Y1)*(1+X2-X1), (X-X1) + (Y+1-Y1)*(1+X2-X1)
						,(X-1-X1) + (Y-1-Y1)*(1+X2-X1), (X+1-X1) + (Y+1-Y1)*(1+X2-X1), (X+1-X1) + (Y-1-Y1)*(1+X2-X1), (X-1-X1) + (Y+1-Y1)*(1+X2-X1)
					};

					// Dissolving...				
					FLOAT DissolvedAmount = DissolvingRatio * WaterData(Center) * It.Value();
					if (DissolvedAmount > 0 && HeightData(Center) >= DissolvedAmount)
					{
						HeightData(Center) -= DissolvedAmount;
						SedimentData(Center) += DissolvedAmount;
					}

					TotalHeightDiff = 0;
					TotalAltitudeDiff = 0;
					TotalWaterDiff = 0;
					TotalSedimentDiff = 0;

					UINT Altitude = HeightData(Center)+WaterData(Center);
					AverageAltitude = 0;
					UINT LowerNeighbor = 0;
					for (INT Idx = 0; Idx < NeighborNum; Idx++)
					{
						UINT NeighborAltitude = HeightData(Neighbor[Idx])+WaterData(Neighbor[Idx]);
						if (Altitude > NeighborAltitude)
						{
							AltitudeDiff[Idx] = Altitude - NeighborAltitude;
							TotalAltitudeDiff += AltitudeDiff[Idx];
							LowerNeighbor++;
							AverageAltitude += NeighborAltitude;
							if (HeightData(Center) > HeightData(Neighbor[Idx]))
								TotalHeightDiff += HeightData(Center) - HeightData(Neighbor[Idx]);
						}
						else
						{
							AltitudeDiff[Idx] = 0;
						}
					}

					// Water Transfer
					if (LowerNeighbor > 0)
					{
						AverageAltitude /= (LowerNeighbor);
						// This is not mathematically correct, but makes good result
						if (TotalHeightDiff)
							AverageAltitude *= (1.f - 0.1 * EdMode->UISettings.GetToolStrength() * Pressure);
							//AverageAltitude -= 4000.f * EdMode->UISettings.GetToolStrength();

						UINT WaterTransfer = Min<UINT>(WaterData(Center), Altitude - (UINT)AverageAltitude) * It.Value();

						for (INT Idx = 0; Idx < NeighborNum; Idx++)
						{
							if (AltitudeDiff[Idx] > 0)
							{
								WaterDiff[Idx] = (UINT)(WaterTransfer * (FLOAT)AltitudeDiff[Idx] / TotalAltitudeDiff);
								WaterData(Neighbor[Idx]) += WaterDiff[Idx];
								TotalWaterDiff += WaterDiff[Idx];
								UINT SedimentDiff = (UINT)(SedimentData(Center) * (FLOAT)WaterDiff[Idx] / WaterData(Center));
								SedimentData(Neighbor[Idx]) += SedimentDiff;
								TotalSedimentDiff += SedimentDiff;
							}
						}

						WaterData(Center) -= TotalWaterDiff;
						SedimentData(Center) -= TotalSedimentDiff;
					}

					// evaporation
					if (WaterData(Center) > 0)
					{
						bWaterExist = TRUE;
						WaterData(Center) = (WORD)(WaterData(Center) * (1.f - EvaporateRatio));
						FLOAT SedimentCap = SedimentCapacity*WaterData(Center);
						FLOAT SedimentDiff = SedimentData(Center) - SedimentCap;
						if (SedimentDiff > 0)
						{
							SedimentData(Center) -= SedimentDiff;
							HeightData(Center) = Clamp<WORD>(HeightData(Center)+SedimentDiff, 0, 65535);
						}
					}
				}	
			}

			if (!bWaterExist) 
			{
				break;
			}
		}

		if (EdMode->UISettings.GetbHErosionDetailSmooth())
			//LowPassFilter<WORD>(X1, Y1, X2, Y2, BrushInfo, HeightData, EdMode->UISettings.GetHErosionDetailScale(), EdMode->UISettings.GetToolStrength() * Pressure);
			LowPassFilter<WORD>(X1, Y1, X2, Y2, BrushInfo, HeightData, EdMode->UISettings.GetHErosionDetailScale(), 1.0f);

		HeightCache->SetCachedData(X1,Y1,X2,Y2,HeightData);
		HeightCache->Flush();
		/*
		if (bWeightApplied)
		{
			WeightCache->SetCachedData(X1,Y1,X2,Y2,WeightDatas, LayerNum);
			WeightCache->Flush();
		}
		*/
	}
};

// 
// FLandscapeToolNoise
//
template<class ToolTarget>
class FLandscapeToolNoise : public FLandscapeToolPaintBase<ToolTarget>
{
public:
	FLandscapeToolNoise(class FEdModeLandscape* InEdMode)
		:	FLandscapeToolPaintBase(InEdMode)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Noise"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Noise"); };

	virtual void ApplyTool(FEditorLevelViewportClient* ViewportClient)
	{
		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2);

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		Cache->CacheData(X1,Y1,X2,Y2);
		TArray<ToolTarget::CacheClass::DataType> Data;
		Cache->GetCachedData(X1,Y1,X2,Y2,Data);

		FLOAT BrushSizeAdjust = 1.0f;
		if (ToolTarget::TargetType != LET_Weightmap && EdMode->UISettings.GetBrushRadius() < EdMode->UISettings.GetMaximumValueRadius())
		{
			BrushSizeAdjust = EdMode->UISettings.GetBrushRadius() / EdMode->UISettings.GetMaximumValueRadius() / 10.f;
		}

		// Apply the brush
		for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
		{
			INT X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);

			if( It.Value() > 0.f )
			{
				FLOAT OriginalValue = Data((X-X1) + (Y-Y1)*(1+X2-X1));
				FLOAT TotalStrength = It.Value() * EdMode->UISettings.GetToolStrength() * Pressure * ToolTarget::StrengthMultiplier(Landscape);
				FNoiseParameter NoiseParam(0, EdMode->UISettings.GetNoiseScale(),  TotalStrength * BrushSizeAdjust);
				FLOAT PaintAmount = ELandscapeToolNoiseMode::Conversion(EdMode->UISettings.GetNoiseMode(), NoiseParam.NoiseAmount, NoiseParam.Sample(X, Y)); //NoiseParam.Sample(X, Y);
				Data((X-X1) + (Y-Y1)*(1+X2-X1)) = ToolTarget::CacheClass::ClampValue(OriginalValue + PaintAmount);
			}	
		}

		Cache->SetCachedData(X1,Y1,X2,Y2,Data);
		Cache->Flush();
	}
};

// 
// FLandscapeToolSelect
//
class FLandscapeToolSelect : public FLandscapeTool
{
public:
	FLandscapeToolSelect(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	Landscape(NULL)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Selection"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Selection"); };

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return TRUE; // applied to all...
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		Landscape = InTarget.Landscape;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		Landscape = NULL;
		bToolActive = FALSE;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, HitX, HitY)  )
		{
			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
			}

			if( bToolActive )
			{
				// Apply tool
				ApplyTool(ViewportClient);
			}
		}

		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;

		if( Landscape )
		{
			// Get list of verts to update
			TMap<QWORD, FLOAT> BrushInfo;
			INT X1, Y1, X2, Y2;
			EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2);

			// Invert when holding Shift
			UBOOL bInvert = IsShiftDown(ViewportClient->Viewport);

			// Todo hold selection... static?
			TSet<ULandscapeComponent*> NewComponents;
			// Select only one component
			INT ComponentIndexX = X1 / Landscape->ComponentSizeQuads;
			INT ComponentIndexY = Y1 / Landscape->ComponentSizeQuads;
			ULandscapeComponent* Component = Landscape->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*Landscape->ComponentSizeQuads,ComponentIndexY*Landscape->ComponentSizeQuads));
			if (Component)
			{
				NewComponents.Add(Component);
			}
			//Landscape->GetComponentsInRegion(X1+1,Y1+1,X2-1,Y2-1,NewComponents);

			TSet<ULandscapeComponent*> NewSelection;

			if (bInvert)
			{
				NewSelection = ALandscape::SelectedComponents.Difference(NewComponents);
			}
			else
			{
				NewSelection = ALandscape::SelectedComponents.Union(NewComponents);
			}

			ALandscape::UpdateSelection(Landscape, NewSelection);
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ALandscape* Landscape;

	UBOOL bToolActive;
};

// 
// FLandscapeToolMoveToLevel
//
class FLandscapeToolMoveToLevel : public FLandscapeTool
{
public:
	FLandscapeToolMoveToLevel(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	Landscape(NULL)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("MoveToLevel"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_MoveToLevel"); };

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return TRUE; // applied to all...
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		Landscape = InTarget.Landscape;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		Landscape = NULL;
		bToolActive = FALSE;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;

		if( Landscape && GEditor && ALandscape::SelectedComponents.Num() && Landscape->GetOuter() != GWorld->CurrentLevel)
		{
			GWarn->BeginSlowTask( TEXT("Moving Landscape components to current level"), TRUE);

			TSet<UTexture2D*> OldTextureSet;
			TArray<ULandscapeComponent*> HeightmapUpdateComponents;
			TMap<UTexture2D*, INT> TextureUnusedCount;
			INT Progress = 0;
			ALandscape::SortSelection();
			INT TotalProgress = ALandscape::SelectedComponents.Num() + ALandscape::SelectedCollisionComponents.Num();
			INT ComponentSizeVerts = Landscape->NumSubsections * (Landscape->SubsectionSizeQuads+1);
			INT NeedHeightmapSize = 1<<appCeilLogTwo( ComponentSizeVerts );

			Landscape->Modify();

			// Check which ones are need for height map change
			for(TSet<ULandscapeComponent*>::TConstIterator It(ALandscape::SelectedComponents);It;++It)
			{
				ULandscapeComponent* Comp = *It;
				Comp->Modify();
				OldTextureSet.Add(Comp->HeightmapTexture);
				// Skip if size is same as components...
				if ( Comp && Comp->HeightmapTexture && NeedHeightmapSize == Comp->HeightmapTexture->Mips(0).SizeX )
				{
					continue;
				}

				INT* UnUsedCount = TextureUnusedCount.Find(Comp->HeightmapTexture);
				if ( UnUsedCount )
				{
					TextureUnusedCount.Set(Comp->HeightmapTexture, *UnUsedCount - 1);
				}
				else
				{
					INT Count = (Comp->HeightmapTexture->Mips(0).SizeX / NeedHeightmapSize) * (Comp->HeightmapTexture->Mips(0).SizeY / NeedHeightmapSize);
					TextureUnusedCount.Set(Comp->HeightmapTexture, Count - 1);
				}
			}

			for(TSet<ULandscapeComponent*>::TConstIterator It(ALandscape::SelectedComponents);It;++It)
			{
				ULandscapeComponent* Comp = *It;
				INT* UnUsedCount = TextureUnusedCount.Find(Comp->HeightmapTexture);
				if (UnUsedCount && *UnUsedCount != 0)
				{
					// need to split height map
					HeightmapUpdateComponents.AddItem(Comp);
				}
			}

			for ( TMap<UTexture2D*, INT>::TIterator It(TextureUnusedCount); It; ++It )
			{
				if (It.Value() == 0)
				{
					// Move heightmap to new level
					UTexture2D* HeightmapTexture = It.Key();
					if (HeightmapTexture->GetOuter() != GWorld->CurrentLevel->GetOutermost())
					{
						HeightmapTexture->Rename(NULL, GWorld->CurrentLevel->GetOutermost());
					}
				}
			}

			// Changing Heightmap format for selected components
			//for(TSet<ULandscapeComponent*>::TConstIterator It(HeightmapChangeComponents);It;++It)
			for (INT Idx = 0; Idx < HeightmapUpdateComponents.Num(); Idx++)
			{
				ULandscapeComponent* Comp = HeightmapUpdateComponents(Idx);
				// make sure the heightmap UVs are powers of two.
				INT HeightmapSizeU = (1<<appCeilLogTwo( ComponentSizeVerts ));
				INT HeightmapSizeV = (1<<appCeilLogTwo( ComponentSizeVerts ));

				UTexture2D* HeightmapTexture;
				TArray<FColor*> HeightmapTextureMipData;
				//Comp->SetupActor();
				{
					// Read old data and split
					FLandscapeEditDataInterface LandscapeEdit(Landscape);
					TArray<BYTE> HeightData;
					HeightData.AddZeroed((1+Landscape->ComponentSizeQuads)*(1+Landscape->ComponentSizeQuads)*sizeof(WORD));
					// Because of edge problem, normal would be just copy from old component data
					TArray<BYTE> NormalData;
					NormalData.AddZeroed((1+Landscape->ComponentSizeQuads)*(1+Landscape->ComponentSizeQuads)*sizeof(WORD));
					LandscapeEdit.GetHeightDataFast(Comp->SectionBaseX, Comp->SectionBaseY, Comp->SectionBaseX+Landscape->ComponentSizeQuads, Comp->SectionBaseY+Landscape->ComponentSizeQuads, (WORD*)&HeightData(0), 0, (WORD*)&NormalData(0));

					// Construct the heightmap textures
					HeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GWorld->CurrentLevel->GetOutermost(), NAME_None, RF_Public|RF_Standalone);
					HeightmapTexture->Init(HeightmapSizeU, HeightmapSizeV, PF_A8R8G8B8);
					HeightmapTexture->SRGB = FALSE;
					HeightmapTexture->CompressionNone = TRUE;
					HeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
					HeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Heightmap;
					HeightmapTexture->AddressX = TA_Clamp;
					HeightmapTexture->AddressY = TA_Clamp;

					INT MipSubsectionSizeQuads = Landscape->SubsectionSizeQuads;
					INT MipSizeU = HeightmapSizeU;
					INT MipSizeV = HeightmapSizeV;
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
						}
						else
						{
							HeightmapTextureData = (FColor*)HeightmapTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
						}

						appMemzero( HeightmapTextureData, MipSizeU*MipSizeV*sizeof(FColor) );
						HeightmapTextureMipData.AddItem(HeightmapTextureData);

						MipSizeU >>= 1;
						MipSizeV >>= 1;

						MipSubsectionSizeQuads = ((MipSubsectionSizeQuads + 1) >> 1) - 1;
					}

					Comp->HeightmapScaleBias = FVector4( 1.f / (FLOAT)HeightmapSizeU, 1.f / (FLOAT)HeightmapSizeV, 0.f, 0.f);
					Comp->HeightmapSubsectionOffset =  (FLOAT)(Landscape->SubsectionSizeQuads+1) / (FLOAT)HeightmapSizeU;
					Comp->HeightmapTexture = HeightmapTexture;
					Comp->LayerUVPan = FVector2D( 0.f, 0.f );

					if (Comp->MaterialInstance)
					{
						Comp->MaterialInstance->SetTextureParameterValue(FName(TEXT("Heightmap")), HeightmapTexture);
					}

					for( INT i=0;i<HeightmapTextureMipData.Num();i++ )
					{
						HeightmapTexture->Mips(i).Data.Unlock();
					}
					LandscapeEdit.SetHeightData(Comp->SectionBaseX, Comp->SectionBaseY, Comp->SectionBaseX+Landscape->ComponentSizeQuads, Comp->SectionBaseY+Landscape->ComponentSizeQuads, (WORD*)&HeightData(0), 0, FALSE, (WORD*)&NormalData(0));
				}
				// End of LandscapeEdit interface
				HeightmapTexture->UpdateResource();
				// Reattach
				FComponentReattachContext ReattachContext(Comp);
			}

			TSet<UTexture2D*> UsedTextureSet;
			// Find reference for Old Heightmap Textures
			for (INT Idx = 0; Idx < Landscape->LandscapeComponents.Num(); Idx++)
			{
				if (Landscape->LandscapeComponents(Idx))
				{
					UsedTextureSet.Add((Landscape->LandscapeComponents(Idx))->HeightmapTexture);
				}
			}

			TSet<UTexture2D*> NoReferencedTextures = OldTextureSet.Difference(UsedTextureSet);
			// Assume that all the height map references are invalid now....
			for(TSet<UTexture2D*>::TIterator It(NoReferencedTextures);It;++It)
			{
				(*It)->SetFlags(RF_Transactional);
				(*It)->Modify();
				(*It)->MarkPackageDirty();
				(*It)->ClearFlags(RF_Standalone);
				//(*It)->ConditionalDestroy();
				//(*It)->MarkPendingKill();
			}

			ALandscapeProxy* LandscapeProxy = NULL;
			// Find there is already a LandscapeProxy Actor
			for (FActorIterator It; It; ++It)
			{
				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
				if (Proxy && Proxy->GetOuter() == GWorld->CurrentLevel && Proxy->bIsProxy && Proxy->LandscapeActor == Landscape)
				{
					LandscapeProxy = Proxy;
					break;
				}
			}

			if (!LandscapeProxy)
			{
				LandscapeProxy = Cast<ALandscapeProxy>( GWorld->SpawnActor( ALandscapeProxy::StaticClass(), NAME_None ) );
			}

			LandscapeProxy->Modify();
			LandscapeProxy->MarkPackageDirty();

			LandscapeProxy->LandscapeActor = Landscape;
			LandscapeProxy->LandscapeActorName = Landscape->GetPathName();
			LandscapeProxy->GetSharedProperties(Landscape);

			// Change Weight maps...
			{
				FLandscapeEditDataInterface LandscapeEdit(Landscape);
				for(TSet<ULandscapeComponent*>::TConstIterator It(ALandscape::SelectedComponents);It;++It)
				{
					ULandscapeComponent* Comp = *It;
					INT TotalNeededChannels = Comp->WeightmapLayerAllocations.Num();
					INT CurrentLayer = 0;
					TArray<UTexture2D*> NewWeightmapTextures;

					// Code from ULandscapeComponent::ReallocateWeightmaps
					// Move to other channels left
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
							for( TMap<UTexture2D*,struct FLandscapeWeightmapUsage>::TIterator It(LandscapeProxy->WeightmapUsageMap); It; ++It )
							{
								FLandscapeWeightmapUsage* TryWeightmapUsage = &It.Value();
								if( TryWeightmapUsage->FreeChannelCount() >= TotalNeededChannels )
								{
									// See if this candidate is closer than any others we've found
									for( INT ChanIdx=0;ChanIdx<4;ChanIdx++ )
									{
										if( TryWeightmapUsage->ChannelUsage[ChanIdx] != NULL  )
										{
											INT TryDistanceSquared = Square(TryWeightmapUsage->ChannelUsage[ChanIdx]->SectionBaseX - Comp->SectionBaseX) + Square(TryWeightmapUsage->ChannelUsage[ChanIdx]->SectionBaseX - Comp->SectionBaseX);
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
							Comp->MarkPackageDirty();

							// Weightmap is sized the same as the component
							INT WeightmapSize = (Comp->SubsectionSizeQuads+1) * Comp->NumSubsections;

							// We need a new weightmap texture
							CurrentWeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GWorld->CurrentLevel->GetOutermost(), NAME_None, RF_Public|RF_Standalone);
							CurrentWeightmapTexture->Init(WeightmapSize,WeightmapSize,PF_A8R8G8B8);
							CurrentWeightmapTexture->SRGB = FALSE;
							CurrentWeightmapTexture->CompressionNone = TRUE;
							CurrentWeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
							CurrentWeightmapTexture->AddressX = TA_Clamp;
							CurrentWeightmapTexture->AddressY = TA_Clamp;
							CurrentWeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Weightmap;
							// Alloc dummy mips
							Comp->CreateEmptyWeightmapMips(CurrentWeightmapTexture);
							CurrentWeightmapTexture->UpdateResource();

							// Store it in the usage map
							CurrentWeightmapUsage = &LandscapeProxy->WeightmapUsageMap.Set(CurrentWeightmapTexture, FLandscapeWeightmapUsage());

							// debugf(TEXT("Making a new texture %s"), *CurrentWeightmapTexture->GetName());
						}

						NewWeightmapTextures.AddItem(CurrentWeightmapTexture);

						for( INT ChanIdx=0;ChanIdx<4 && TotalNeededChannels > 0;ChanIdx++ )
						{
							// debugf(TEXT("Finding allocation for layer %d"), CurrentLayer);

							if( CurrentWeightmapUsage->ChannelUsage[ChanIdx] == NULL  )
							{
								// Use this allocation
								FWeightmapLayerAllocationInfo& AllocInfo = Comp->WeightmapLayerAllocations(CurrentLayer);

								if( AllocInfo.WeightmapTextureIndex == 255 )
								{
									// New layer - zero out the data for this texture channel
									LandscapeEdit.ZeroTextureChannel( CurrentWeightmapTexture, ChanIdx );
								}
								else
								{
									UTexture2D* OldWeightmapTexture = Comp->WeightmapTextures(AllocInfo.WeightmapTextureIndex);

									// Copy the data
									LandscapeEdit.CopyTextureChannel( CurrentWeightmapTexture, ChanIdx, OldWeightmapTexture, AllocInfo.WeightmapTextureChannel );
									LandscapeEdit.ZeroTextureChannel( OldWeightmapTexture, AllocInfo.WeightmapTextureChannel );

									// Remove the old allocation
									FLandscapeWeightmapUsage* OldWeightmapUsage = Landscape->WeightmapUsageMap.Find(OldWeightmapTexture);
									OldWeightmapUsage->ChannelUsage[AllocInfo.WeightmapTextureChannel] = NULL;
								}

								// Assign the new allocation
								CurrentWeightmapUsage->ChannelUsage[ChanIdx] = Comp;
								AllocInfo.WeightmapTextureIndex = NewWeightmapTextures.Num()-1;
								AllocInfo.WeightmapTextureChannel = ChanIdx;
								CurrentLayer++;
								TotalNeededChannels--;
							}
						}
					}

					// Replace the weightmap textures
					Comp->WeightmapTextures = NewWeightmapTextures;

					// Update the mipmaps for the textures we edited
					for( INT Idx=0;Idx<Comp->WeightmapTextures.Num();Idx++)
					{
						UTexture2D* WeightmapTexture = Comp->WeightmapTextures(Idx);
						FLandscapeTextureDataInfo* WeightmapDataInfo = LandscapeEdit.GetTextureDataInfo(WeightmapTexture);

						INT NumMips = WeightmapTexture->Mips.Num();
						TArray<FColor*> WeightmapTextureMipData(NumMips);
						for( INT MipIdx=0;MipIdx<NumMips;MipIdx++ )
						{
							WeightmapTextureMipData(MipIdx) = (FColor*)WeightmapDataInfo->GetMipData(MipIdx);
						}

						ULandscapeComponent::UpdateWeightmapMips(Comp->NumSubsections, Comp->SubsectionSizeQuads, WeightmapTexture, WeightmapTextureMipData, 0, 0, MAXINT, MAXINT, WeightmapDataInfo);
					}
				}
				// Need to Repacking all the Weight map (to make it packed well...)
				Landscape->RemoveInvalidWeightmaps();
			}

			for(TSet<ULandscapeComponent*>::TConstIterator It(ALandscape::SelectedComponents);It;++It)
			{
				// Need to move or recreate all related data (Height map, Weight map, maybe collision components, allocation info)
				ULandscapeComponent* Comp = *It;
				//Landscape->XYtoComponentMap.Remove(ALandscape::MakeKey(Comp->SectionBaseX, Comp->SectionBaseY));
				Landscape->LandscapeComponents.RemoveItem(Comp);
				Comp->InvalidateLightingCache();
				Comp->Rename(NULL, LandscapeProxy);
				LandscapeProxy->LandscapeComponents.AddItem( Comp );
				Comp->UpdateMaterialInstances();	
				FComponentReattachContext ReattachContext(Comp);
				//Comp->ConditionalDetach();
				//Comp->ConditionalAttach(GWorld->Scene, LandscapeProxy, Landscape->LocalToWorld());

				GWarn->StatusUpdatef( Progress++, TotalProgress, *FString::Printf( TEXT("Moving Component : %s"), *Comp->GetName() ) );
			}

			for(TSet<ULandscapeHeightfieldCollisionComponent*>::TConstIterator It(ALandscape::SelectedCollisionComponents);It;++It)
			{
				// Need to move or recreate all related data (Height map, Weight map, maybe collision components, allocation info)
				ULandscapeHeightfieldCollisionComponent* Comp = *It;
				//Landscape->XYtoCollisionComponentMap.Remove(ALandscape::MakeKey(Comp->SectionBaseX,Comp->SectionBaseY));
				Landscape->CollisionComponents.RemoveItem(*It);
				Comp->Rename(NULL, LandscapeProxy);
				LandscapeProxy->CollisionComponents.AddItem(*It);
				//Comp->ConditionalDetach();
				//Comp->ConditionalAttach(GWorld->Scene, LandscapeProxy, Landscape->LocalToWorld());

				GWarn->StatusUpdatef( Progress++, TotalProgress, *FString::Printf( TEXT("Moving Component : %s"), *Comp->GetName() ) );
			}

			// Update our new components
			LandscapeProxy->ConditionalUpdateComponents();
			// Init RB physics for editor collision
			LandscapeProxy->InitRBPhysEditor();

			GEditor->SelectNone(FALSE, TRUE);
			GEditor->SelectActor( LandscapeProxy, TRUE, NULL, FALSE, TRUE );
			const UBOOL bUseCurrentLevelGridVolume = FALSE;
			GEditor->MoveSelectedActorsToCurrentLevel( bUseCurrentLevelGridVolume );

			GEditor->SelectNone(FALSE, TRUE);

			GWarn->EndSlowTask();

			// Remove Selection
			TSet<ULandscapeComponent*> NewSelection;
			ALandscape::UpdateSelection(Landscape, NewSelection);
			ALandscape::SelectedCollisionComponents.Empty();
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ALandscape* Landscape;

	UBOOL bToolActive;
};

// 
// FLandscapeToolAddComponent
//
class FLandscapeToolAddComponent : public FLandscapeTool
{
public:
	FLandscapeToolAddComponent(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	Landscape(NULL)
		,	HeightCache(NULL)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("AddComponent"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_AddComponent"); };

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return TRUE; // applied to all...
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		Landscape = InTarget.Landscape;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);

		HeightCache = new FLandscapeHeightCache(InTarget);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		delete HeightCache;
		HeightCache = NULL;
		Landscape = NULL;
		bToolActive = FALSE;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, HitX, HitY)  )
		{
			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
			}

			if( bToolActive )
			{
				// Apply tool
				ApplyTool(ViewportClient);
			}
		}

		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;

		if( Landscape && GLandscapeRenderAddCollision)
		{
			// Get list of verts to update
			TMap<QWORD, FLOAT> BrushInfo;
			INT X1, Y1, X2, Y2;
			EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2);

			// expand the area to get valid data from regions...
			X1 -= 1;
			Y1 -= 1;
			X2 += 1;
			Y2 += 1;

			HeightCache->CacheData(X1,Y1,X2,Y2);
			TArray<WORD> Data;
			HeightCache->GetCachedData(X1,Y1,X2,Y2,Data);

			// Find component range for this block of data, non shared vertices
			INT ComponentIndexX1 = (X1+1 >= 0) ? (X1+1) / Landscape->ComponentSizeQuads : (X1+2) / Landscape->ComponentSizeQuads - 1;
			INT ComponentIndexY1 = (Y1+1 >= 0) ? (Y1+1) / Landscape->ComponentSizeQuads : (Y1+2) / Landscape->ComponentSizeQuads - 1;
			INT ComponentIndexX2 = (X2-2 >= 0) ? (X2-2) / Landscape->ComponentSizeQuads : (X2-1) / Landscape->ComponentSizeQuads - 1;
			INT ComponentIndexY2 = (Y2-2 >= 0) ? (Y2-2) / Landscape->ComponentSizeQuads : (Y2-1) / Landscape->ComponentSizeQuads - 1;

			TArray<ULandscapeComponent*> NewComponents;
			Landscape->Modify();
			for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
			{
				for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
				{		
					ULandscapeComponent* Component = Landscape->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*Landscape->ComponentSizeQuads,ComponentIndexY*Landscape->ComponentSizeQuads));
					if( !Component )
					{
						// Add New component...
						ULandscapeComponent* LandscapeComponent = ConstructObject<ULandscapeComponent>(ULandscapeComponent::StaticClass(), Landscape, NAME_None, RF_Transactional);
						Landscape->LandscapeComponents.AddItem(LandscapeComponent);
						NewComponents.AddItem(LandscapeComponent);
						LandscapeComponent->Init(
							ComponentIndexX*Landscape->ComponentSizeQuads,ComponentIndexY*Landscape->ComponentSizeQuads,
							Landscape->ComponentSizeQuads,
							Landscape->NumSubsections,
							Landscape->SubsectionSizeQuads
							);

						INT ComponentVerts = (Landscape->SubsectionSizeQuads+1) * Landscape->NumSubsections;
						TArray<FColor> HeightData;
						HeightData.Empty( Square(ComponentVerts) );
						HeightData.AddZeroed( Square(ComponentVerts) );
						LandscapeComponent->InitHeightmapData(HeightData);
						LandscapeComponent->UpdateMaterialInstances();
					}
				}
			}

			HeightCache->SetCachedData(X1,Y1,X2,Y2,Data);
			HeightCache->Flush();

			for ( INT Idx = 0; Idx < NewComponents.Num(); Idx++ )
			{
				QWORD Key = ALandscape::MakeKey(NewComponents(Idx)->SectionBaseX, NewComponents(Idx)->SectionBaseY);
				NewComponents(Idx)->UpdateComponent(GWorld->Scene, Landscape, Landscape->LocalToWorld());
				ULandscapeHeightfieldCollisionComponent* Comp = Landscape->XYtoCollisionComponentMap.FindRef( Key );
				if (Comp)
				{
					Comp->UpdateComponent(GWorld->Scene, Landscape, Landscape->LocalToWorld());
					Comp->RecreateHeightfield();
				}
			}
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ALandscape* Landscape;

	FLandscapeHeightCache* HeightCache;
	UBOOL bToolActive;
};

// 
// FLandscapeToolDeleteComponent
//
class FLandscapeToolDeleteComponent : public FLandscapeTool
{
public:
	FLandscapeToolDeleteComponent(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	Landscape(NULL)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("DeleteComponent"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_DeleteComponent"); };

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return TRUE; // applied to all...
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		Landscape = InTarget.Landscape;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		Landscape = NULL;
		bToolActive = FALSE;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		ALandscape* Landscape = EdMode->CurrentToolTarget.Landscape;

		if( Landscape && ALandscape::SelectedComponents.Num())
		{
			Landscape->Modify();

			TArray<QWORD> DeletedNeighborKeys;
			// Check which ones are need for height map change
			for(TSet<ULandscapeComponent*>::TIterator It(ALandscape::SelectedComponents);It;++It)
			{
				ULandscapeComponent* Comp = *It;
				ALandscapeProxy* Proxy = Comp->GetLandscapeProxy();
				Proxy->Modify();
				Comp->Modify();
				if (Comp->HeightmapTexture)
				{
					Comp->HeightmapTexture->SetFlags(RF_Transactional);
					Comp->HeightmapTexture->Modify();
					Comp->HeightmapTexture->MarkPackageDirty();
					Comp->HeightmapTexture->ClearFlags(RF_Standalone); // Remove when there is no reference for this Heightmap...
				}

				for (INT i = 0; i < Comp->WeightmapTextures.Num(); ++i)
				{
					Comp->WeightmapTextures(i)->SetFlags(RF_Transactional);
					Comp->WeightmapTextures(i)->Modify();
					Comp->WeightmapTextures(i)->MarkPackageDirty();
					Comp->WeightmapTextures(i)->ClearFlags(RF_Standalone);
				}

				QWORD Key = ALandscape::MakeKey(Comp->SectionBaseX, Comp->SectionBaseY);
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX-Comp->ComponentSizeQuads,	Comp->SectionBaseY-Comp->ComponentSizeQuads));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX,							Comp->SectionBaseY-Comp->ComponentSizeQuads));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX+Comp->ComponentSizeQuads,	Comp->SectionBaseY-Comp->ComponentSizeQuads));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX-Comp->ComponentSizeQuads,	Comp->SectionBaseY));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX+Comp->ComponentSizeQuads,	Comp->SectionBaseY));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX-Comp->ComponentSizeQuads,	Comp->SectionBaseY+Comp->ComponentSizeQuads));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX,							Comp->SectionBaseY+Comp->ComponentSizeQuads));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX+Comp->ComponentSizeQuads,	Comp->SectionBaseY+Comp->ComponentSizeQuads));

				Proxy->LandscapeComponents.RemoveItem(Comp);
				ULandscapeHeightfieldCollisionComponent* CollisionComp = Landscape->XYtoCollisionComponentMap.FindRef(Key);
				if (CollisionComp)
				{
					Proxy->CollisionComponents.RemoveItem(CollisionComp);
					Landscape->XYtoCollisionComponentMap.Remove(Key);
					CollisionComp->ConditionalDetach();
				}
				Landscape->XYtoComponentMap.Remove(Key);
				Comp->ConditionalDetach();
			}

			// Update AddCollisions...
			for (INT i = 0; i < DeletedNeighborKeys.Num(); ++i)
			{
				Landscape->XYtoAddCollisionMap.Remove(DeletedNeighborKeys(i));
			}

			for (INT i = 0; i < DeletedNeighborKeys.Num(); ++i)
			{
				ULandscapeHeightfieldCollisionComponent* CollisionComp = Landscape->XYtoCollisionComponentMap.FindRef(DeletedNeighborKeys(i));
				if (CollisionComp)
				{
					CollisionComp->UpdateAddCollisions();
				}
			}

			// Remove Selection
			TSet<ULandscapeComponent*> NewSelection;
			ALandscape::UpdateSelection(Landscape, NewSelection);
			ALandscape::SelectedCollisionComponents.Empty();
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ALandscape* Landscape;

	UBOOL bToolActive;
};

//
// FEdModeLandscape
//

/** Constructor */
FEdModeLandscape::FEdModeLandscape() 
:	FEdMode()
,	bToolActive(FALSE)
{
	ID = EM_Landscape;
	Desc = TEXT( "Landscape" );

	// Initialize tools.
	FLandscapeToolSet* ToolSet_Paint = new FLandscapeToolSet(TEXT("ToolSet_Paint"));
	ToolSet_Paint->AddTool(new FLandscapeToolPaint<FHeightmapToolTarget>(this));
	ToolSet_Paint->AddTool(new FLandscapeToolPaint<FWeightmapToolTarget>(this));
	LandscapeToolSets.AddItem(ToolSet_Paint);

	FLandscapeToolSet* ToolSet_Smooth = new FLandscapeToolSet(TEXT("ToolSet_Smooth"));
	ToolSet_Smooth->AddTool(new FLandscapeToolSmooth<FHeightmapToolTarget>(this));
	ToolSet_Smooth->AddTool(new FLandscapeToolSmooth<FWeightmapToolTarget>(this));
	LandscapeToolSets.AddItem(ToolSet_Smooth);

	FLandscapeToolSet* ToolSet_Flatten = new FLandscapeToolSet(TEXT("ToolSet_Flatten"));
	ToolSet_Flatten->AddTool(new FLandscapeToolFlatten<FHeightmapToolTarget>(this));
	ToolSet_Flatten->AddTool(new FLandscapeToolFlatten<FWeightmapToolTarget>(this));
	LandscapeToolSets.AddItem(ToolSet_Flatten);

	FLandscapeToolSet* ToolSet_Erosion = new FLandscapeToolSet(TEXT("ToolSet_Erosion"));
	ToolSet_Erosion->AddTool(new FLandscapeToolErosion(this));
	LandscapeToolSets.AddItem(ToolSet_Erosion);

	FLandscapeToolSet* ToolSet_HydraErosion = new FLandscapeToolSet(TEXT("ToolSet_HydraErosion"));
	ToolSet_HydraErosion->AddTool(new FLandscapeToolHydraErosion(this));
	LandscapeToolSets.AddItem(ToolSet_HydraErosion);

	FLandscapeToolSet* ToolSet_Noise = new FLandscapeToolSet(TEXT("ToolSet_Noise"));
	ToolSet_Noise->AddTool(new FLandscapeToolNoise<FHeightmapToolTarget>(this));
	ToolSet_Noise->AddTool(new FLandscapeToolNoise<FWeightmapToolTarget>(this));
	LandscapeToolSets.AddItem(ToolSet_Noise);

	FLandscapeToolSet* ToolSet_Select = new FLandscapeToolSet(TEXT("ToolSet_Select"));
	ToolSet_Select->AddTool(new FLandscapeToolSelect(this));
	LandscapeToolSets.AddItem(ToolSet_Select);

	FLandscapeToolSet* ToolSet_MoveToLevel = new FLandscapeToolSet(TEXT("ToolSet_MoveToLevel"));
	ToolSet_MoveToLevel->AddTool(new FLandscapeToolMoveToLevel(this));
	LandscapeToolSets.AddItem(ToolSet_MoveToLevel);

	AddComponentToolIndex = LandscapeToolSets.Num();
	FLandscapeToolSet* ToolSet_AddComponent = new FLandscapeToolSet(TEXT("ToolSet_AddComponent"));
	ToolSet_AddComponent->AddTool(new FLandscapeToolAddComponent(this));
	LandscapeToolSets.AddItem(ToolSet_AddComponent);

	FLandscapeToolSet* ToolSet_DeleteComponent = new FLandscapeToolSet(TEXT("ToolSet_DeleteComponent"));
	ToolSet_DeleteComponent->AddTool(new FLandscapeToolDeleteComponent(this));
	LandscapeToolSets.AddItem(ToolSet_DeleteComponent);

	CurrentToolSet = NULL;
	CurrentToolIndex = -1;

	// Initialize brushes

	FLandscapeBrushSet* BrushSet; 
	BrushSet = new(LandscapeBrushSets) FLandscapeBrushSet(TEXT("BrushSet_Circle"), *LocalizeUnrealEd("LandscapeMode_Brush_Circle"));
	BrushSet->Brushes.AddItem(new FLandscapeBrushCircle_Smooth(this));
	BrushSet->Brushes.AddItem(new FLandscapeBrushCircle_Linear(this));
	BrushSet->Brushes.AddItem(new FLandscapeBrushCircle_Spherical(this));
	BrushSet->Brushes.AddItem(new FLandscapeBrushCircle_Tip(this));

	BrushSet = new(LandscapeBrushSets) FLandscapeBrushSet(TEXT("BrushSet_Alpha"), *LocalizeUnrealEd("LandscapeMode_Brush_Alpha"));
	BrushSet->Brushes.AddItem(new FLandscapeBrushAlphaPattern(this));

	BrushSet = new(LandscapeBrushSets) FLandscapeBrushSet(TEXT("BrushSet_Selection"), *LocalizeUnrealEd("LandscapeMode_Brush_Selection"));
	BrushSet->Brushes.AddItem(new FLandscapeBrushSelection(this));

	CurrentBrush = LandscapeBrushSets(0).Brushes(0);

	CurrentToolTarget.Landscape = NULL;
	CurrentToolTarget.TargetType = LET_Heightmap;
	CurrentToolTarget.LayerName = NAME_None;
}


/** Destructor */
FEdModeLandscape::~FEdModeLandscape()
{
	// Save UI settings to config file
	UISettings.Save();

	delete CurrentBrush;

	// Destroy tools.
	for( INT ToolIdx=0;ToolIdx<LandscapeToolSets.Num();ToolIdx++ )
	{
		delete LandscapeToolSets(ToolIdx);
	}
	LandscapeToolSets.Empty();

	// Destroy brushes
	for( INT BrushSetIdx=0;BrushSetIdx<LandscapeBrushSets.Num();BrushSetIdx++ )
	{
		FLandscapeBrushSet& BrushSet = LandscapeBrushSets(BrushSetIdx);

		for( INT BrushIdx=0;BrushIdx < BrushSet.Brushes.Num();BrushIdx++ )
		{
			delete BrushSet.Brushes(BrushIdx);
		}
	}
	LandscapeBrushSets.Empty();
}



/** FSerializableObject: Serializer */
void FEdModeLandscape::Serialize( FArchive &Ar )
{
	// Call parent implementation
	FEdMode::Serialize( Ar );
}

/** FEdMode: Called when the mode is entered */
void FEdModeLandscape::Enter()
{
	// Call parent implementation
	FEdMode::Enter();

	// Load UI settings from config file
	UISettings.Load();

#if WITH_MANAGED_CODE
	// Create the mesh paint window
	HWND EditorFrameWindowHandle = (HWND)GApp->EditorFrame->GetHandle();
	LandscapeEditWindow.Reset( FLandscapeEditWindow::CreateLandscapeEditWindow( this, EditorFrameWindowHandle ) );
	check( LandscapeEditWindow.IsValid() );
#endif

	// Force real-time viewports.  We'll back up the current viewport state so we can restore it when the
	// user exits this mode.
	const UBOOL bWantRealTime = TRUE;
	const UBOOL bRememberCurrentState = TRUE;
	ForceRealTimeViewports( bWantRealTime, bRememberCurrentState );

	CurrentBrush->EnterBrush();

	SetCurrentTool(CurrentToolIndex >= 0 ? CurrentToolIndex : 0);
}


/** FEdMode: Called when the mode is exited */
void FEdModeLandscape::Exit()
{
	// Restore real-time viewport state if we changed it
	const UBOOL bWantRealTime = FALSE;
	const UBOOL bRememberCurrentState = FALSE;
	ForceRealTimeViewports( bWantRealTime, bRememberCurrentState );

	// Save any settings that may have changed
#if WITH_MANAGED_CODE
	if( LandscapeEditWindow.IsValid() )
	{
		LandscapeEditWindow->SaveWindowSettings();
	}

	// Kill the mesh paint window
	LandscapeEditWindow.Reset();
#endif

	CurrentBrush->LeaveBrush();

	// Save UI settings to config file
	UISettings.Save();
	GLandscapeViewMode = ELandscapeViewMode::Normal;

	// Call parent implementation
	FEdMode::Exit();
}

/** FEdMode: Called once per frame */
void FEdModeLandscape::Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
{
	FEdMode::Tick(ViewportClient,DeltaTime);

	if( CurrentToolSet && CurrentToolSet->GetTool() )
	{
		CurrentToolSet->GetTool()->Tick(ViewportClient,DeltaTime);
	}
	if( CurrentBrush )
	{
		CurrentBrush->Tick(ViewportClient,DeltaTime);
	}
}


/** FEdMode: Called when the mouse is moved over the viewport */
UBOOL FEdModeLandscape::MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT MouseX, INT MouseY )
{
	UBOOL Result = FALSE;
	if( CurrentToolSet && CurrentToolSet->GetTool() )
	{
		Result = CurrentToolSet && CurrentToolSet->GetTool()->MouseMove(ViewportClient, Viewport, MouseX, MouseY);
		ViewportClient->Invalidate( FALSE, FALSE );
	}
	return Result;
}

/**
 * Called when the mouse is moved while a window input capture is in effect
 *
 * @param	InViewportClient	Level editor viewport client that captured the mouse input
 * @param	InViewport			Viewport that captured the mouse input
 * @param	InMouseX			New mouse cursor X coordinate
 * @param	InMouseY			New mouse cursor Y coordinate
 *
 * @return	TRUE if input was handled
 */
UBOOL FEdModeLandscape::CapturedMouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT MouseX, INT MouseY )
{
	UBOOL Result = FALSE;
	if( CurrentToolSet && CurrentToolSet->GetTool() )
	{
		Result = CurrentToolSet && CurrentToolSet->GetTool()->CapturedMouseMove(ViewportClient, Viewport, MouseX, MouseY);
		ViewportClient->Invalidate( FALSE, FALSE );
	}
	return Result;
}


/** FEdMode: Called when a mouse button is pressed */
UBOOL FEdModeLandscape::StartTracking()
{
	return TRUE;
}



/** FEdMode: Called when the a mouse button is released */
UBOOL FEdModeLandscape::EndTracking()
{
	return TRUE;
}

namespace
{
	UBOOL RayIntersectTriangle(const FVector& Start, const FVector& End, const FVector& A, const FVector& B, const FVector& C, FVector& IntersectPoint )
	{
		const FVector BA = A - B;
		const FVector CB = B - C;
		const FVector TriNormal = BA ^ CB;

		UBOOL bCollide = SegmentPlaneIntersection(Start, End, FPlane(A, TriNormal), IntersectPoint );
		if (!bCollide)
		{
			return FALSE;
		}

		FVector BaryCentric = ComputeBaryCentric2D(IntersectPoint, A, B, C);
		if (BaryCentric.X > 0.f && BaryCentric.Y > 0.f && BaryCentric.Z > 0.f )
		{
			return TRUE;
		}
		return FALSE;
	}
};

/** Trace under the mouse cursor and return the landscape hit and the hit location (in landscape quad space) */
UBOOL FEdModeLandscape::LandscapeMouseTrace( FEditorLevelViewportClient* ViewportClient, FLOAT& OutHitX, FLOAT& OutHitY )
{
	INT MouseX = ViewportClient->Viewport->GetMouseX();
	INT MouseY = ViewportClient->Viewport->GetMouseY();

	// Compute a world space ray from the screen space mouse coordinates
	FSceneViewFamilyContext ViewFamily(
		ViewportClient->Viewport, ViewportClient->GetScene(),
		ViewportClient->ShowFlags,
		GWorld->GetTimeSeconds(),
		GWorld->GetDeltaSeconds(),
		GWorld->GetRealTimeSeconds(),
		ViewportClient->IsRealtime()
		);
	FSceneView* View = ViewportClient->CalcSceneView( &ViewFamily );
	FViewportCursorLocation MouseViewportRay( View, ViewportClient, MouseX, MouseY );

	FVector Start = MouseViewportRay.GetOrigin();
	FVector End = Start + WORLD_MAX * MouseViewportRay.GetDirection();

	FMemMark		Mark(GMainThreadMemStack);
	FCheckResult*	FirstHit	= NULL;
	DWORD			TraceFlags	= TRACE_Terrain|TRACE_TerrainIgnoreHoles;

	FirstHit	= GWorld->MultiLineCheck(GMainThreadMemStack, End, Start, FVector(0.f,0.f,0.f), TraceFlags, NULL);
	for( FCheckResult* Test = FirstHit; Test; Test = Test->GetNext() )
	{
		ULandscapeHeightfieldCollisionComponent* CollisionComponent = Cast<ULandscapeHeightfieldCollisionComponent>(Test->Component);
		if( CollisionComponent )
		{
			ALandscape* HitLandscape = CollisionComponent->GetLandscapeActor(); //CastChecked<ALandscape>(CollisionComponent->GetOuter());		

			if( HitLandscape && HitLandscape == CurrentToolTarget.Landscape )
			{
				FVector DrawScale = HitLandscape->DrawScale3D * HitLandscape->DrawScale;

				OutHitX = (Test->Location.X-HitLandscape->Location.X) / DrawScale.X;
				OutHitY = (Test->Location.Y-HitLandscape->Location.Y) / DrawScale.X;

				Mark.Pop();

				return TRUE;
			}
		}
	}

	// For Add Landscape Component Mode
	if (CurrentToolIndex == AddComponentToolIndex && CurrentToolTarget.Landscape)
	{
		UBOOL bCollided = FALSE;
		FVector IntersectPoint;
		GLandscapeRenderAddCollision = NULL;
		// Need to optimize collision for AddLandscapeComponent...?
		for (TMap<QWORD, FLandscapeAddCollision>::TIterator It(CurrentToolTarget.Landscape->XYtoAddCollisionMap); It; ++It )
		{
			FLandscapeAddCollision& AddCollision = It.Value();
			// Triangle 1
			bCollided = RayIntersectTriangle(Start, End, AddCollision.Corners[0], AddCollision.Corners[3], AddCollision.Corners[1], IntersectPoint );
			if (bCollided)
			{
				GLandscapeRenderAddCollision = &AddCollision;
				break;
			}
			// Triangle 2
			bCollided = RayIntersectTriangle(Start, End, AddCollision.Corners[0], AddCollision.Corners[2], AddCollision.Corners[3], IntersectPoint );
			if (bCollided)
			{
				GLandscapeRenderAddCollision = &AddCollision;
				break;
			}
		}
		if (bCollided)
		{
			FVector DrawScale = CurrentToolTarget.Landscape->DrawScale3D * CurrentToolTarget.Landscape->DrawScale;

			OutHitX = (IntersectPoint.X - CurrentToolTarget.Landscape->Location.X) / DrawScale.X;
			OutHitY = (IntersectPoint.Y - CurrentToolTarget.Landscape->Location.Y) / DrawScale.X;

			Mark.Pop();

			return TRUE;
		}
	}

	Mark.Pop();
	return FALSE;
}


/** FEdMode: Called when a key is pressed */
UBOOL FEdModeLandscape::InputKey( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, FName Key, EInputEvent Event )
{
	// Override Key Input for Selection Brush
	if( CurrentBrush && CurrentBrush->InputKey(ViewportClient, Viewport, Key, Event)==TRUE )
	{
		return TRUE;
	}

	if( Key == KEY_LeftMouseButton )
	{
		if( Event == IE_Pressed && (IsCtrlDown(Viewport) || (Viewport->IsPenActive() && Viewport->GetTabletPressure() > 0.f)) )
		{
			if( CurrentToolSet )
			{				
				if( CurrentToolSet->SetToolForTarget(CurrentToolTarget) )
				{
					FLOAT HitX, HitY;
					if( LandscapeMouseTrace(ViewportClient, HitX, HitY) )
					{
						bToolActive = CurrentToolSet->GetTool()->BeginTool(ViewportClient, CurrentToolTarget, HitX, HitY);
					}
				}
			}
			return TRUE;
		}
	}

	if( Key == KEY_LeftMouseButton || Key==KEY_LeftControl || Key==KEY_RightControl )
	{
		if( Event == IE_Released && CurrentToolSet && CurrentToolSet->GetTool() && bToolActive )
		{
			CurrentToolSet->GetTool()->EndTool();
			bToolActive = FALSE;
			return TRUE;
		}
	}

	// Prev tool 
	if( Event == IE_Pressed && Key == KEY_Comma )
	{
		if( CurrentToolSet && CurrentToolSet->GetTool() && bToolActive )
		{
			CurrentToolSet->GetTool()->EndTool();
			bToolActive = FALSE;
		}

		INT NewToolIndex = LandscapeToolSets.FindItemIndex(CurrentToolSet) - 1;
		SetCurrentTool( LandscapeToolSets.IsValidIndex(NewToolIndex) ? NewToolIndex : LandscapeToolSets.Num()-1 );

		return TRUE;
	}

	// Next tool 
	if( Event == IE_Pressed && Key == KEY_Period )
	{
		if( CurrentToolSet && CurrentToolSet->GetTool() && bToolActive )
		{
			CurrentToolSet->GetTool()->EndTool();
			bToolActive = FALSE;
		}

		INT NewToolIndex = LandscapeToolSets.FindItemIndex(CurrentToolSet) + 1;
		SetCurrentTool( LandscapeToolSets.IsValidIndex(NewToolIndex) ? NewToolIndex : 0 );

		return TRUE;
	}

	if( CurrentToolSet && CurrentToolSet->GetTool() && CurrentToolSet->GetTool()->InputKey(ViewportClient, Viewport, Key, Event)==TRUE )
	{
		return TRUE;
	}

	if( CurrentBrush && CurrentBrush->InputKey(ViewportClient, Viewport, Key, Event)==TRUE )
	{
		return TRUE;
	}

	return FALSE;
}

void FEdModeLandscape::SetCurrentTool( INT ToolIndex )
{
	CurrentToolIndex = LandscapeToolSets.IsValidIndex(ToolIndex) ? ToolIndex : 0;
	CurrentToolSet = LandscapeToolSets( ToolIndex );
	CurrentToolSet->SetToolForTarget( CurrentToolTarget );

#if WITH_MANAGED_CODE
	if( LandscapeEditWindow.IsValid() )
	{
		LandscapeEditWindow->NotifyCurrentToolChanged(ToolIndex);
	}
#endif
}

void FEdModeLandscape::GetLayersAndThumbnails( TArray<FLandscapeLayerThumbnailInfo>& OutLayerTumbnailInfo )
{
	OutLayerTumbnailInfo.Empty();
	
	if( CurrentToolTarget.Landscape )
	{
		UTexture2D* ThumbnailWeightmap = NULL;
		UTexture2D* ThumbnailHeightmap = NULL;

		for( INT LayerIdx=0;LayerIdx<CurrentToolTarget.Landscape->LayerInfos.Num(); LayerIdx++ )
		{
			FLandscapeLayerInfo& LayerInfo = CurrentToolTarget.Landscape->LayerInfos(LayerIdx);
			FName LayerName = LayerInfo.LayerName;

			FLandscapeLayerThumbnailInfo* ThumbnailInfo = new(OutLayerTumbnailInfo) FLandscapeLayerThumbnailInfo(LayerInfo);

			if( LayerInfo.ThumbnailMIC == NULL )
			{
				if( ThumbnailWeightmap == NULL )
				{
					ThumbnailWeightmap = LoadObject<UTexture2D>(NULL, TEXT("EditorLandscapeResources.LandscapeThumbnailWeightmap"), NULL, LOAD_None, NULL);
				}
				if( ThumbnailHeightmap == NULL )
				{
					ThumbnailHeightmap = LoadObject<UTexture2D>(NULL, TEXT("EditorLandscapeResources.LandscapeThumbnailHeightmap"), NULL, LOAD_None, NULL);
				}

				// Construct Thumbnail MIC
				LayerInfo.ThumbnailMIC = ConstructObject<UMaterialInstanceConstant>(UMaterialInstanceConstant::StaticClass(), CurrentToolTarget.Landscape->GetOutermost(), NAME_None, RF_Public|RF_Standalone);
				LayerInfo.ThumbnailMIC->SetParent(CurrentToolTarget.Landscape->LandscapeMaterial);
				FStaticParameterSet StaticParameters;
				LayerInfo.ThumbnailMIC->GetStaticParameterValues(&StaticParameters);

				for( INT LayerParameterIdx=0;LayerParameterIdx<StaticParameters.TerrainLayerWeightParameters.Num();LayerParameterIdx++ )
				{
					FStaticTerrainLayerWeightParameter& LayerParameter = StaticParameters.TerrainLayerWeightParameters(LayerParameterIdx);
					if( LayerParameter.ParameterName == LayerName )
					{
						LayerParameter.WeightmapIndex = 0;
						LayerParameter.bOverride = TRUE;
					}
					else
					{
						LayerParameter.WeightmapIndex = INDEX_NONE;
					}
				}

				LayerInfo.ThumbnailMIC->SetStaticParameterValues(&StaticParameters);
				LayerInfo.ThumbnailMIC->InitResources();
				LayerInfo.ThumbnailMIC->UpdateStaticPermutation();
				LayerInfo.ThumbnailMIC->SetTextureParameterValue(FName("Weightmap0"), ThumbnailWeightmap); 
				LayerInfo.ThumbnailMIC->SetTextureParameterValue(FName("Heightmap"), ThumbnailHeightmap);
			}

			const FObjectThumbnail* LayerThumbnail = ThumbnailTools::FindCachedThumbnail( LayerInfo.ThumbnailMIC->GetFullName() );

			// If we didn't find it in memory, OR if the thumbnail needs to be refreshed...
			if( LayerThumbnail == NULL || LayerThumbnail->IsEmpty() || LayerThumbnail->IsDirty() )
			{
				// Generate a thumbnail
				LayerThumbnail = ThumbnailTools::GenerateThumbnailForObject( LayerInfo.ThumbnailMIC );
			}

			if( LayerThumbnail != NULL )
			{
				ThumbnailInfo->LayerThumbnail = *LayerThumbnail;
			}
		}
	}
}

/** FEdMode: Called when mouse drag input it applied */
UBOOL FEdModeLandscape::InputDelta( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale )
{
	return FALSE;
}



/** FEdMode: Render the mesh paint tool */
void FEdModeLandscape::Render( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	/** Call parent implementation */
	FEdMode::Render( View, Viewport, PDI );

	if (GLandscapeRenderAddCollision)
	{
		PDI->DrawLine(GLandscapeRenderAddCollision->Corners[0], GLandscapeRenderAddCollision->Corners[3], FColor(0, 255, 128), SDPG_World);
		PDI->DrawLine(GLandscapeRenderAddCollision->Corners[3], GLandscapeRenderAddCollision->Corners[1], FColor(0, 255, 128), SDPG_World);
		PDI->DrawLine(GLandscapeRenderAddCollision->Corners[1], GLandscapeRenderAddCollision->Corners[0], FColor(0, 255, 128), SDPG_World);

		PDI->DrawLine(GLandscapeRenderAddCollision->Corners[0], GLandscapeRenderAddCollision->Corners[2], FColor(0, 255, 128), SDPG_World);
		PDI->DrawLine(GLandscapeRenderAddCollision->Corners[2], GLandscapeRenderAddCollision->Corners[3], FColor(0, 255, 128), SDPG_World);
		PDI->DrawLine(GLandscapeRenderAddCollision->Corners[3], GLandscapeRenderAddCollision->Corners[0], FColor(0, 255, 128), SDPG_World);
	}
}



/** FEdMode: Render HUD elements for this tool */
void FEdModeLandscape::DrawHUD( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas )
{

}



/** FEdMode: Called when the currently selected actor has changed */
void FEdModeLandscape::ActorSelectionChangeNotify()
{
}



/** Forces real-time perspective viewports */
void FEdModeLandscape::ForceRealTimeViewports( const UBOOL bEnable, const UBOOL bStoreCurrentState )
{
	// Force perspective views to be real-time
	for( INT CurViewportIndex = 0; CurViewportIndex < GApp->EditorFrame->ViewportConfigData->GetViewportCount(); ++CurViewportIndex )
	{
		WxLevelViewportWindow* CurLevelViewportWindow =
			GApp->EditorFrame->ViewportConfigData->AccessViewport( CurViewportIndex ).ViewportWindow;
		if( CurLevelViewportWindow != NULL )
		{
			if( CurLevelViewportWindow->ViewportType == LVT_Perspective )
			{				
				if( bEnable )
				{
					CurLevelViewportWindow->SetRealtime( bEnable, bStoreCurrentState );
				}
				else
				{
					CurLevelViewportWindow->RestoreRealtime(TRUE);
				}
			}
		}
	}
}

/** Load UI settings from ini file */
void FLandscapeUISettings::Load()
{
	FString WindowPositionString;
	if( GConfig->GetString( TEXT("LandscapeEdit"), TEXT("WindowPosition"), WindowPositionString, GEditorUserSettingsIni ) )
	{
		TArray<FString> PositionValues;
		if( WindowPositionString.ParseIntoArray( &PositionValues, TEXT( "," ), TRUE ) == 4 )
		{
			WindowX = appAtoi( *PositionValues(0) );
			WindowY = appAtoi( *PositionValues(1) );
			WindowWidth = appAtoi( *PositionValues(2) );
			WindowHeight = appAtoi( *PositionValues(3) );
		}
	}

	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("ToolStrength"), ToolStrength, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("BrushRadius"), BrushRadius, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("BrushFalloff"), BrushFalloff, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushScale"), AlphaBrushScale, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushRotation"), AlphaBrushRotation, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushPanU"), AlphaBrushPanU, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushPanV"), AlphaBrushPanV, GEditorUserSettingsIni );
	GConfig->GetString( TEXT("LandscapeEdit"), TEXT("AlphaTextureName"), AlphaTextureName, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("AlphaTextureChannel"), AlphaTextureChannel, GEditorUserSettingsIni );
	SetAlphaTexture(*AlphaTextureName, AlphaTextureChannel);

	INT InFlattenMode = ELandscapeToolNoiseMode::Both; 
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("FlattenMode"), InFlattenMode, GEditorUserSettingsIni );
	FlattenMode = (ELandscapeToolNoiseMode::Type)InFlattenMode;

	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("ErodeThresh"), ErodeThresh, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("ErodeIterationNum"), ErodeIterationNum, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("ErodeSurfaceThickness"), ErodeSurfaceThickness, GEditorUserSettingsIni );
	INT InErosionNoiseMode = ELandscapeToolNoiseMode::Sub; 
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("ErosionNoiseMode"), InErosionNoiseMode, GEditorUserSettingsIni );
	ErosionNoiseMode = (ELandscapeToolNoiseMode::Type)InErosionNoiseMode;
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("ErosionNoiseScale"), ErosionNoiseScale, GEditorUserSettingsIni );

	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("RainAmount"), RainAmount, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("SedimentCapacity"), SedimentCapacity, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("HErodeIterationNum"), HErodeIterationNum, GEditorUserSettingsIni );
	INT InRainDistMode = ELandscapeToolNoiseMode::Both; 
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("RainDistNoiseMode"), InRainDistMode, GEditorUserSettingsIni );
	RainDistMode = (ELandscapeToolNoiseMode::Type)InRainDistMode;
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("RainDistScale"), RainDistScale, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("HErosionDetailScale"), HErosionDetailScale, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bHErosionDetailSmooth"), bHErosionDetailSmooth, GEditorUserSettingsIni );

	INT InNoiseMode = ELandscapeToolNoiseMode::Both; 
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("NoiseMode"), InNoiseMode, GEditorUserSettingsIni );
	NoiseMode = (ELandscapeToolNoiseMode::Type)InNoiseMode;
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("NoiseScale"), NoiseScale, GEditorUserSettingsIni );

	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("DetailScale"), DetailScale, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bDetailSmooth"), bDetailSmooth, GEditorUserSettingsIni );

	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("MaximumValueRadius"), MaximumValueRadius, GEditorUserSettingsIni );
}

/** Save UI settings to ini file */
void FLandscapeUISettings::Save()
{
	FString WindowPositionString = FString::Printf(TEXT("%d,%d,%d,%d"), WindowX, WindowY, WindowWidth, WindowHeight );
	GConfig->SetString( TEXT("LandscapeEdit"), TEXT("WindowPosition"), *WindowPositionString, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("ToolStrength"), ToolStrength, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("BrushRadius"), BrushRadius, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("BrushFalloff"), BrushFalloff, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushScale"), AlphaBrushScale, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushRotation"), AlphaBrushRotation, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushPanU"), AlphaBrushPanU, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushPanV"), AlphaBrushPanV, GEditorUserSettingsIni );
	GConfig->SetString( TEXT("LandscapeEdit"), TEXT("AlphaTextureName"), *AlphaTextureName, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("AlphaTextureChannel"), AlphaTextureChannel, GEditorUserSettingsIni );

	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("FlattenMode"), FlattenMode, GEditorUserSettingsIni );

	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("ErodeThresh"), ErodeThresh, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("ErodeIterationNum"), ErodeIterationNum, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("ErodeSurfaceThickness"), ErodeSurfaceThickness, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("ErosionNoiseMode"), (INT)ErosionNoiseMode, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("ErosionNoiseScale"), ErosionNoiseScale, GEditorUserSettingsIni );

	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("RainAmount"), RainAmount, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("SedimentCapacity"), SedimentCapacity, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("HErodeIterationNum"), ErodeIterationNum, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("RainDistMode"), (INT)RainDistMode, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("RainDistScale"), RainDistScale, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("HErosionDetailScale"), HErosionDetailScale, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bHErosionDetailSmooth"), bHErosionDetailSmooth, GEditorUserSettingsIni );

	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("NoiseMode"), (INT)NoiseMode, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("NoiseScale"), NoiseScale, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("DetailScale"), DetailScale, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bDetailSmooth"), bDetailSmooth, GEditorUserSettingsIni );

	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("MaximumValueRadius"), MaximumValueRadius, GEditorUserSettingsIni );
}

UBOOL FLandscapeUISettings::SetAlphaTexture(const TCHAR* InTextureName, INT InTextureChannel)
{
	UBOOL Result = TRUE;

	if( AlphaTexture )
	{
		AlphaTexture->RemoveFromRoot();
	}

	TArray<BYTE> NewTextureData;

	// Try to load specified texture.
	UTexture2D* NewAlphaTexture = NULL;
	if( InTextureName != NULL) 
	{
		NewAlphaTexture = LoadObject<UTexture2D>(NULL, InTextureName, NULL, LOAD_None, NULL);
	}

	// No texture or no source art, try to use the previous texture.
	if( NewAlphaTexture == NULL || !NewAlphaTexture->HasSourceArt() )
	{
		NewAlphaTexture = AlphaTexture;
		Result = FALSE;
	}

	// Use the previous texture
	if( NewAlphaTexture != NULL && NewAlphaTexture->HasSourceArt() )
	{
		NewAlphaTexture->GetUncompressedSourceArt(NewTextureData);
	}

	// Load fallback if there's no texture or data
	if( NewAlphaTexture == NULL || (NewTextureData.Num() != 4 * NewAlphaTexture->SizeX * NewAlphaTexture->SizeY) )
	{
		NewAlphaTexture = LoadObject<UTexture2D>(NULL, TEXT("EditorLandscapeResources.DefaultAlphaTexture"), NULL, LOAD_None, NULL);
		NewAlphaTexture->GetUncompressedSourceArt(NewTextureData);
		Result = FALSE;
	}

	check( NewAlphaTexture );
	AlphaTexture = NewAlphaTexture;
	AlphaTextureName = AlphaTexture->GetPathName();
	AlphaTextureSizeX = NewAlphaTexture->SizeX;
	AlphaTextureSizeY = NewAlphaTexture->SizeY;
	AlphaTextureChannel = InTextureChannel;
	AlphaTexture->AddToRoot();
	check( NewTextureData.Num() == 4 *AlphaTextureSizeX*AlphaTextureSizeY );

	AlphaTextureData.Empty(AlphaTextureSizeX*AlphaTextureSizeY);

	FLOAT UnpackMin;
	FLOAT UnpackMax;
	BYTE* SrcPtr;
	switch(AlphaTextureChannel)
	{
	case 1:
		SrcPtr = &((FColor*)&NewTextureData(0))->G;
		UnpackMin = NewAlphaTexture->UnpackMin[1];
		UnpackMax = NewAlphaTexture->UnpackMax[1];
		break;
	case 2:
		SrcPtr = &((FColor*)&NewTextureData(0))->B;
		UnpackMin = NewAlphaTexture->UnpackMin[2];
		UnpackMax = NewAlphaTexture->UnpackMax[2];
		break;
	case 3:
		SrcPtr = &((FColor*)&NewTextureData(0))->A;
		UnpackMin = NewAlphaTexture->UnpackMin[3];
		UnpackMax = NewAlphaTexture->UnpackMax[3];
		break;
	default:
		SrcPtr = &((FColor*)&NewTextureData(0))->R;
		UnpackMin = NewAlphaTexture->UnpackMin[0];
		UnpackMax = NewAlphaTexture->UnpackMax[0];
		break;
	}

	for( INT i=0;i<AlphaTextureSizeX*AlphaTextureSizeY;i++ )
	{
		BYTE Value = Clamp<INT>(appRound( 255.f * (UnpackMin + (UnpackMax-UnpackMin) * (FLOAT)(*SrcPtr) / 255.f)), 0, 255);
		AlphaTextureData.AddItem(Value);
		SrcPtr += 4;
	}

	return Result;
}