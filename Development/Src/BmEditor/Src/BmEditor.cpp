//=============================================================================
// Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
// This is where you would extend the editor with game-specific dependencies
// and functionality
//=============================================================================

#if _WINDOWS
#include "UnrealEd.h"
#include "BmEditorClasses.h"

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName BMEDITOR_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "BmEditorClasses.h"
#undef AUTOGENERATE_NAME

#undef AUTOGENERATE_FUNCTION
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "BmEditorClasses.h"
#undef AUTOGENERATE_NAME

#undef AUTOGENERATE_FUNCTION
#undef NATIVES_ONLY
#undef NAMES_ONLY

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsBmEditor( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_BMEDITOR;
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesBmEditor()
{
	#define NAMES_ONLY
    #define AUTOGENERATE_NAME(name) BMEDITOR_##name = FName(TEXT(#name));
// 		#include "BmEditorNames.h"
	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "BmEditorClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
}

#if CHECK_NATIVE_CLASS_SIZES
#if _MSC_VER
#pragma optimize( "", off )
#endif

void AutoCheckNativeClassSizesBmEditor( UBOOL& Mismatch )
{
#define NAMES_ONLY
#define AUTOGENERATE_NAME( name )
#define AUTOGENERATE_FUNCTION( cls, idx, name )
#define VERIFY_CLASS_SIZES
#include "BmEditorClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY
#undef VERIFY_CLASS_SIZES
}

#if _MSC_VER
#pragma optimize( "", on )
#endif
#endif

IMPLEMENT_CLASS(UUIContainerThumbnailRenderer);

#endif
