/*=============================================================================
	RSniperPoint.cpp
	BM: Native implementation for BM2's RSniperPoint.

	Sniper points are wired up by the editor's "Link Actors" tooling: linking two
	sniper points makes them alternates for each other, linking a sniper point to
	anything else makes that actor something to aim at.

	PostEditSelect is ported 1:1; LinkToActor and UnlinkToActor are reconstructed,
	since RSniperPoint's script bodies didn't ship (they're stubs in the retail
	package, like RClimbLocator's).
=============================================================================*/

#include "BmGame.h"

IMPLEMENT_CLASS_EXTENSION(ARSniperPoint, "BmGame.RSniperPoint");

TExtensionProperty<TArray<ARSniperPoint*> >	ARSniperPoint::LinkedSniperPointsProp;
TExtensionProperty<TArray<AActor*> >		ARSniperPoint::AimPointsProp;

UBOOL ARSniperPoint::BindProperties()
{
	UClass* Cls = GetClass();

	LinkedSniperPointsProp.Bind(Cls, TEXT("LinkedSniperPoints"));
	AimPointsProp.Bind(Cls, TEXT("AimPoints"));

	return LinkedSniperPointsProp.IsBound() && AimPointsProp.IsBound();
}

UBOOL ARSniperPoint::IsSniperPoint( AActor* Actor )
{
	static UClass* SniperPointClass = NULL;
	if( !SniperPointClass )
	{
		SniperPointClass = FindObject<UClass>(NULL, TEXT("BmGame.RSniperPoint"));
	}

	return Actor && SniperPointClass && Actor->IsA(SniperPointClass);
}

void ARSniperPoint::AddLinkedSniperPoint( ARSniperPoint* LinkTarget )
{
	if( LinkTarget != this && LinkedSniperPoints().FindItemIndex(LinkTarget) == INDEX_NONE )
	{
		LinkedSniperPoints().AddItem(LinkTarget);
	}
}

void ARSniperPoint::RemoveLinkedSniperPoint( ARSniperPoint* LinkTarget )
{
	const INT Index = LinkedSniperPoints().FindItemIndex(LinkTarget);
	if( Index != INDEX_NONE )
	{
		LinkedSniperPoints().Remove(Index, 1);
	}
}

void ARSniperPoint::MarkAsDirty()
{
	// Script reaches the reattach by bouncing bHidden; from here we can just ask for it.
	MarkComponentsAsDirty(FALSE);
}

UBOOL ARSniperPoint::LinkToActor( AActor* LinkTarget )
{
	// A lone sniper point has nothing to link against, so leave its links alone.
	if( !LinkTarget || !BindProperties() )
	{
		return FALSE;
	}

	if( IsSniperPoint(LinkTarget) )
	{
		ARSniperPoint* OtherPoint = (ARSniperPoint*)LinkTarget;
		if( !OtherPoint->BindProperties() )
		{
			return FALSE;
		}

		AddLinkedSniperPoint(OtherPoint);
		OtherPoint->AddLinkedSniperPoint(this);
		OtherPoint->MarkAsDirty();
	}
	else if( AimPoints().FindItemIndex(LinkTarget) == INDEX_NONE )
	{
		AimPoints().AddItem(LinkTarget);
	}

	MarkAsDirty();
	return TRUE;
}

UBOOL ARSniperPoint::UnlinkToActor( AActor* UnlinkTarget )
{
	if( !UnlinkTarget || !BindProperties() )
	{
		return FALSE;
	}

	if( IsSniperPoint(UnlinkTarget) )
	{
		ARSniperPoint* OtherPoint = (ARSniperPoint*)UnlinkTarget;
		if( !OtherPoint->BindProperties() )
		{
			return FALSE;
		}

		RemoveLinkedSniperPoint(OtherPoint);
		OtherPoint->RemoveLinkedSniperPoint(this);
		OtherPoint->MarkAsDirty();
	}
	else
	{
		const INT Index = AimPoints().FindItemIndex(UnlinkTarget);
		if( Index == INDEX_NONE )
		{
			return FALSE;
		}

		AimPoints().Remove(Index, 1);
	}

	MarkAsDirty();
	return TRUE;
}

void ARSniperPoint::PostEditSelect( UBOOL bSelected )
{
	if( !BindProperties() )
	{
		return;
	}

	// Aim points that have since been deleted would otherwise stay in the array as NULLs.
	for( INT I=AimPoints().Num()-1; I>=0; I-- )
	{
		if( !AimPoints()(I) )
		{
			AimPoints().Remove(I, 1);
		}
	}
}

// Nothing else references this file, so give the linker a reason to keep the extension above.
void RegisterRSniperPointExtensions()
{
}
