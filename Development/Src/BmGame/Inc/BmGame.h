/*=============================================================================
	ExampleGame.h
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "EngineAnimClasses.h"
#include "EngineMeshClasses.h"
#include "EngineInterpolationClasses.h"
#include "BmGameClasses.h"
// BM
#include "RAnimUtil.h"

#if BATMAN
#include "RSkeletalMeshActor.h"
#include "RHidePoint.h"
#include "RSniperPoint.h"

// BM: registration entry points for extended script classes.
extern void RegisterRSkeletalMeshActorNatives();
extern void RegisterRHidePointNatives();
extern void RegisterRSniperPointExtensions();
#endif



