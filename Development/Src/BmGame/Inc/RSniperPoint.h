/*=============================================================================
	RSniperPoint.h
	BM: Native implementation for BM2's RSniperPoint.
=============================================================================*/

#ifndef RSNIPERPOINT_H
#define RSNIPERPOINT_H

// Declares no data members - see UnClassExtension.h.
class ARSniperPoint : public AActor
{
public:
	// DECLARE_CLASS would normally provide this; we have no UClass of our own.
	typedef AActor Super;

	virtual UBOOL LinkToActor(AActor* LinkTarget);
	virtual UBOOL UnlinkToActor(AActor* UnlinkTarget);
	virtual void PostEditSelect(UBOOL bSelected);

	TArray<ARSniperPoint*>& LinkedSniperPoints()	{ return LinkedSniperPointsProp(this); }
	TArray<AActor*>& AimPoints()					{ return AimPointsProp(this); }

	/** Resolve the script properties we touch. FALSE if the class isn't what we expect. */
	UBOOL BindProperties();

	static UBOOL IsSniperPoint(AActor* Actor);

private:
	void AddLinkedSniperPoint(ARSniperPoint* LinkTarget);
	void RemoveLinkedSniperPoint(ARSniperPoint* LinkTarget);

	void MarkAsDirty();

	static TExtensionProperty<TArray<ARSniperPoint*> >	LinkedSniperPointsProp;
	static TExtensionProperty<TArray<AActor*> >			AimPointsProp;
};

#endif // RSNIPERPOINT_H
