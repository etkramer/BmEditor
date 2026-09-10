/*=============================================================================
	RLevelVolume.h
	BM: Native implementation for BM2's RLevelVolume.
=============================================================================*/

#ifndef RLEVELVOLUME_H
#define RLEVELVOLUME_H

/** Mirrors BmGame.BorderInfo - only present so FVisibleLevelInfo has the right size. */
struct FBorderInfo
{
	FVector2D	EdgeStart;
	FVector2D	EdgeEnd;
};

/** Mirrors BmGame.VisibleLevelInfo - offsets come from the loaded struct, this is only for typed access. */
struct FVisibleLevelInfo
{
	FName				LevelName;
	TArray<FBorderInfo>	Borders;
	FLOAT				RoadHeight;
	FVector2D			EdgeStart;
	FVector2D			EdgeEnd;
};

// Declares no data members - see UnClassExtension.h.
class ARLevelVolume : public AVolume
{
public:
	// DECLARE_CLASS would normally provide this; we have no UClass of our own.
	typedef AVolume Super;

	virtual void Spawned();

	/** Level is always the package the volume was saved into, which is why it's editconst. */
	void UpdateLevelName();

	/** Resolve the script properties we touch. FALSE if the class isn't what we expect. */
	UBOOL BindProperties();

	FName& Level()											{ return LevelProp(this); }
	FLOAT& Priority()										{ return PriorityProp(this); }
	TArray<FVisibleLevelInfo>& OtherLevelsVisibleInfo()		{ return VisibleInfoProp(this); }
	TArray<FVisibleLevelInfo>& OtherLevelLODsVisibleInfo()	{ return LODInfoProp(this); }

	/** RLevelVolume has no UClass of ours to Cast<> against, so identify it by name. */
	static ARLevelVolume* CastFrom(AActor* Actor);

private:
	static TExtensionProperty<FName>						LevelProp;
	static TExtensionProperty<FLOAT>						PriorityProp;
	static TExtensionProperty<TArray<FVisibleLevelInfo> >	VisibleInfoProp;
	static TExtensionProperty<TArray<FVisibleLevelInfo> >	LODInfoProp;
};

#endif // RLEVELVOLUME_H
