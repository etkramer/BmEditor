/*=============================================================================
	RHidePoint.h
	BM: Native implementation for BM2's RHidePoint.
=============================================================================*/

#ifndef RHIDEPOINT_H
#define RHIDEPOINT_H

/** Mirrors BmGame.HideLink - offsets come from the loaded struct, this is only for typed access. */
struct FHideLink
{
	FVector	SwingPosition;
	FLOAT	TravelTime;
	FVector	MidPoint;
};

// Declares no data members - see UnClassExtension.h.
class ARHidePoint : public AFracturedStaticMeshActor
{
public:
	// DECLARE_CLASS would normally provide this; we have no UClass of our own.
	typedef AFracturedStaticMeshActor Super;

	virtual void PostEditMove(UBOOL bFinished);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void CheckForErrors();

	/** Rebuilds this point's swing links. bAdjustNeighbours re-runs the pass on other points in our level. */
	virtual void AutoAdjust(UBOOL bAdjustNeighbours);
	void UpdateIcons();

	ARHidePoint* GetLinkedHidePoint(INT I);
	UBOOL IsHidePointLinked(ARHidePoint* TestPoint);

	/** Resolve the script properties we touch. FALSE if the class isn't what we expect. */
	UBOOL BindProperties();

	DECLARE_FUNCTION(execGetLinkedHidePoint)
	{
		P_GET_INT(I);
		P_FINISH;
		*(ARHidePoint**)Result = GetLinkedHidePoint(I);
	}

	DECLARE_FUNCTION(execIsHidePointLinked)
	{
		P_GET_OBJECT(UObject, TestPoint);
		P_FINISH;
		*(UBOOL*)Result = IsHidePointLinked((ARHidePoint*)TestPoint);
	}

private:
	/** The link arrays are only meaningful through GetLinkedHidePoint, hence the private accessors. */
	UObject*& SwingMove()						{ return SwingMoveProp(this); }
	TArray<FHideLink>& HidePoints()				{ return HidePointsProp(this); }
	TArray<FString>& BlockedHidePoints()		{ return BlockedHidePointsProp(this); }

	static TExtensionProperty<UObject*>			SwingMoveProp;
	static TExtensionProperty<FLOAT>			VantagePointRangeProp;
	static TExtensionProperty<TArray<FHideLink> > HidePointsProp;
	static TExtensionProperty<TArray<FString> >	BlockedHidePointsProp;
	static TExtensionProperty<ARHidePoint*>		LinkedCrossLevelProp[6];
	static TExtensionProperty<ARHidePoint*>		LinkedSameLevelProp;
	static TExtensionProperty<BYTE>				HidePointCrossLevelProp;
	static TExtensionProperty<USpriteComponent*> GoodSpriteProp;
	static TExtensionProperty<USpriteComponent*> BadSpriteProp;
	static FExtensionBoolProperty				ValidHidePointProp;
	static FExtensionBoolProperty				AutoAdjustOnPathBuildProp;
};

#endif // RHIDEPOINT_H
