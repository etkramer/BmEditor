/*=============================================================================
	RLevelVolume.cpp
	BM: Native implementation for BM2's RLevelVolume.

	A volume's Level is simply the package it lives in - stamped on spawn, and
	refreshed in bulk by the rebuild-map pass. Nothing else writes it, which is
	what makes it safe for RPlayerStartInLevel to copy out of.
=============================================================================*/

#include "BmGame.h"

IMPLEMENT_CLASS_EXTENSION(ARLevelVolume, "BmGame.RLevelVolume");

TExtensionProperty<FName>						ARLevelVolume::LevelProp;
TExtensionProperty<FLOAT>						ARLevelVolume::PriorityProp;
TExtensionProperty<TArray<FVisibleLevelInfo> >	ARLevelVolume::VisibleInfoProp;
TExtensionProperty<TArray<FVisibleLevelInfo> >	ARLevelVolume::LODInfoProp;

ARLevelVolume* ARLevelVolume::CastFrom( AActor* Actor )
{
	static UClass* LevelVolumeClass = NULL;
	if( !LevelVolumeClass )
	{
		LevelVolumeClass = FindObject<UClass>(NULL, TEXT("BmGame.RLevelVolume"));
	}

	return (Actor && LevelVolumeClass && Actor->IsA(LevelVolumeClass)) ? (ARLevelVolume*)Actor : NULL;
}

UBOOL ARLevelVolume::BindProperties()
{
	UClass* Cls = GetClass();

	LevelProp.Bind(Cls, TEXT("Level"));
	PriorityProp.Bind(Cls, TEXT("Priority"));
	VisibleInfoProp.Bind(Cls, TEXT("OtherLevelsVisibleInfo"));
	LODInfoProp.Bind(Cls, TEXT("OtherLevelLODsVisibleInfo"));

	// TExtensionProperty only sizes the array itself, so check FVisibleLevelInfo against the loaded struct.
	UArrayProperty* VisibleInfo = FindField<UArrayProperty>(Cls, TEXT("OtherLevelsVisibleInfo"));
	checkf(!VisibleInfo || VisibleInfo->Inner->ElementSize == sizeof(FVisibleLevelInfo),
		TEXT("VisibleLevelInfo is %i bytes in script, expected %i"), VisibleInfo->Inner->ElementSize, (INT)sizeof(FVisibleLevelInfo));

	return LevelProp.IsBound() && PriorityProp.IsBound() && VisibleInfoProp.IsBound() && LODInfoProp.IsBound();
}

void ARLevelVolume::UpdateLevelName()
{
	if( BindProperties() )
	{
		Level() = GetOutermost()->GetFName();
	}
}

void ARLevelVolume::Spawned()
{
	Super::Spawned();
	UpdateLevelName();
}

// Nothing else references this file, so give the linker a reason to keep the extension above.
void RegisterRLevelVolumeExtensions()
{
}
