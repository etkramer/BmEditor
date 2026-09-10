/*=============================================================================
	RPlayerStartInLevel.cpp
	BM: Native implementation for BM2's RPlayerStartInLevel.

	Level and the two streaming level lists aren't authored by hand - they're stamped
	onto the actor from whichever RLevelVolume encloses it whenever it is moved or
	edited. Ported from ARPlayerStartInLevel::UpdateConnectingLevel; the base layer
	unhide pass around the search is what makes volumes in hidden levels findable.
=============================================================================*/

#include "BmGame.h"
#include "LevelUtils.h"

IMPLEMENT_CLASS_EXTENSION(ARPlayerStartInLevel, "BmGame.RPlayerStartInLevel");

TExtensionProperty<FName>			ARPlayerStartInLevel::LevelProp;
TExtensionProperty<TArray<FName> >	ARPlayerStartInLevel::AdditionalStreamingLevelsProp;
TExtensionProperty<TArray<FName> >	ARPlayerStartInLevel::AdditionalStreamingLODLevelsProp;

/**
 * Force-shows the hidden base layers so the volumes living in them can be iterated.
 * OutWasVisible records what to put back, one entry per streaming level; levels we
 * leave alone are marked visible so the restore pass skips them.
 */
static void UnhideBaseLayerLevels( TArray<UBOOL>& OutWasVisible )
{
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	UBOOL bChangedVisibility = FALSE;

	OutWasVisible.Empty(WorldInfo->StreamingLevels.Num());
	OutWasVisible.AddZeroed(WorldInfo->StreamingLevels.Num());

	for( INT I=0; I<WorldInfo->StreamingLevels.Num(); I++ )
	{
		ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(I);
		if( !StreamingLevel || !FLevelUtils::IsABaseLayerName(StreamingLevel->PackageNameAsString) )
		{
			OutWasVisible(I) = TRUE;
			continue;
		}

		OutWasVisible(I) = StreamingLevel->bShouldBeVisibleInEditor;
		if( !OutWasVisible(I) )
		{
			debugf(NAME_Log, TEXT("Unhiding %s"), *StreamingLevel->PackageName.ToString());
			StreamingLevel->bShouldBeVisibleInEditor = TRUE;
			bChangedVisibility = TRUE;
		}
	}

	if( bChangedVisibility )
	{
		GWorld->FlushLevelStreaming(NULL, TRUE);
	}
}

/** Re-hides whatever UnhideBaseLayerLevels showed. */
static void RestoreLevelVisibility( const TArray<UBOOL>& WasVisible )
{
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	UBOOL bChangedVisibility = FALSE;

	for( INT I=0; I<WorldInfo->StreamingLevels.Num() && I<WasVisible.Num(); I++ )
	{
		ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(I);
		if( StreamingLevel && !WasVisible(I) )
		{
			StreamingLevel->bShouldBeVisibleInEditor = FALSE;
			bChangedVisibility = TRUE;
		}
	}

	if( bChangedVisibility )
	{
		GWorld->FlushLevelStreaming(NULL, TRUE);
	}
}

/** The editor half of ARPlayerStartInLevel::GetMyLevelVolume - highest priority volume wins. */
static ARLevelVolume* FindLevelVolumeAt( const FVector& Point )
{
	ARLevelVolume* BestVolume = NULL;
	for( FActorIterator It; It; ++It )
	{
		ARLevelVolume* Volume = ARLevelVolume::CastFrom(*It);
		if( !Volume || !Volume->WorldInfo || !Volume->BindProperties() || !Volume->Encompasses(Point) )
		{
			continue;
		}

		if( !BestVolume || Volume->Priority() > BestVolume->Priority() )
		{
			BestVolume = Volume;
		}
	}

	return BestVolume;
}

UBOOL ARPlayerStartInLevel::BindProperties()
{
	UClass* Cls = GetClass();

	LevelProp.Bind(Cls, TEXT("Level"));
	AdditionalStreamingLevelsProp.Bind(Cls, TEXT("AdditionalStreamingLevels"));
	AdditionalStreamingLODLevelsProp.Bind(Cls, TEXT("AdditionalStreamingLODLevels"));

	return LevelProp.IsBound() && AdditionalStreamingLevelsProp.IsBound() && AdditionalStreamingLODLevelsProp.IsBound();
}

void ARPlayerStartInLevel::UpdateConnectingLevel()
{
	if( !GWorld || !BindProperties() )
	{
		return;
	}

	TArray<UBOOL> WasVisible;
	UnhideBaseLayerLevels(WasVisible);

	TArray<FName> NewStreamingLevels;
	TArray<FName> NewStreamingLODLevels;

	ARLevelVolume* Volume = FindLevelVolumeAt(Location);
	if( Volume )
	{
		if( Level() != Volume->Level() )
		{
			Level() = Volume->Level();
			Modify(TRUE);
		}

		const TArray<FVisibleLevelInfo>& VisibleInfo = Volume->OtherLevelsVisibleInfo();
		for( INT I=0; I<VisibleInfo.Num(); I++ )
		{
			NewStreamingLevels.AddUniqueItem(VisibleInfo(I).LevelName);
		}

		const TArray<FVisibleLevelInfo>& LODInfo = Volume->OtherLevelLODsVisibleInfo();
		for( INT I=0; I<LODInfo.Num(); I++ )
		{
			NewStreamingLODLevels.AddUniqueItem(LODInfo(I).LevelName);
		}
	}
	else if( Level() != NAME_None )
	{
		Level() = NAME_None;
		Modify(TRUE);
	}

	if( AdditionalStreamingLevels() != NewStreamingLevels )
	{
		AdditionalStreamingLevels() = NewStreamingLevels;
		Modify(TRUE);
	}

	if( AdditionalStreamingLODLevels() != NewStreamingLODLevels )
	{
		AdditionalStreamingLODLevels() = NewStreamingLODLevels;
		Modify(TRUE);
	}

	RestoreLevelVisibility(WasVisible);
}

// Retail deliberately doesn't chain to Super here or in PostEditChangeProperty.
void ARPlayerStartInLevel::PostEditMove( UBOOL bFinished )
{
	if( bFinished )
	{
		UpdateConnectingLevel();
	}
}

void ARPlayerStartInLevel::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	if( PropertyChangedEvent.Property )
	{
		UpdateConnectingLevel();
	}
}

void ARPlayerStartInLevel::CheckForErrors()
{
	Super::CheckForErrors();

	if( BindProperties() && Level() == NAME_None )
	{
		GWarn->MapCheck_Add(MCTYPE_WARNING, this,
			*FString::Printf(TEXT("%s: Not connected to a level!"), *GetName()));
	}
}

// Nothing else references this file, so give the linker a reason to keep the extension above.
void RegisterRPlayerStartInLevelExtensions()
{
}
