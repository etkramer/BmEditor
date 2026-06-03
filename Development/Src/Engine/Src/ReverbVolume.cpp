/*=============================================================================
	ReverbVolume.cpp: Used to affect reverb settings in the game and editor.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
 
IMPLEMENT_CLASS( AReverbVolume );

/**
 * Removes the reverb volume to world info's list of reverb volumes.
 */
void AReverbVolume::ClearComponents( void )
{
	// Route clear to super first.
	Super::ClearComponents();
	NextLowerPriorityVolume = NULL;
}

/**
 * Adds the reverb volume to world info's list of reverb volumes.
 */
void AReverbVolume::UpdateComponentsInternal( UBOOL bCollisionUpdate )
{
	// Route update to super first.
	Super::UpdateComponentsInternal( bCollisionUpdate );
	NextLowerPriorityVolume = NULL;
}

/**
 * callback for changed property 
 */
void AReverbVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	Settings.Volume = Clamp<FLOAT>( Settings.Volume, 0.0f, 1.0f );
	AmbientZoneSettings.InteriorTime = Max<FLOAT>( 0.01f, AmbientZoneSettings.InteriorTime );
	AmbientZoneSettings.InteriorLPFTime = Max<FLOAT>( 0.01f, AmbientZoneSettings.InteriorLPFTime );
	AmbientZoneSettings.ExteriorTime = Max<FLOAT>( 0.01f, AmbientZoneSettings.ExteriorTime );
	AmbientZoneSettings.ExteriorLPFTime = Max<FLOAT>( 0.01f, AmbientZoneSettings.ExteriorLPFTime );
}
