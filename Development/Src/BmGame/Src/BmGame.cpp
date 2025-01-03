/*=============================================================================
	BmGame.cpp
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "BmGame.h"

/*-----------------------------------------------------------------------------
	The following must be done once per package.
-----------------------------------------------------------------------------*/

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName BMGAME_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "BmGameClasses.h"
#undef AUTOGENERATE_NAME

#undef AUTOGENERATE_FUNCTION
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "BmGameClasses.h"
#undef AUTOGENERATE_NAME

#undef AUTOGENERATE_FUNCTION
#undef NATIVES_ONLY
#undef NAMES_ONLY

#if CHECK_NATIVE_CLASS_SIZES
#if _MSC_VER
#pragma optimize( "", off )
#endif

void AutoCheckNativeClassSizesBmGame( UBOOL& Mismatch )
{
#define NAMES_ONLY
#define AUTOGENERATE_NAME( name )
#define AUTOGENERATE_FUNCTION( cls, idx, name )
#define VERIFY_CLASS_SIZES
#include "BmGameClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY
#undef VERIFY_CLASS_SIZES
}

#if _MSC_VER
#pragma optimize( "", on )
#endif
#endif

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsBmGame( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_BMGAME;
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesBmGame()
{
	#define NAMES_ONLY
    #define AUTOGENERATE_NAME(name) BMGAME_##name = FName(TEXT(#name));
//		#include "BmGameNames.h"
	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "BmGameClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
}

/**
 * Game-specific code to handle DLC being added or removed
 *
 * @param bContentWasInstalled TRUE if DLC was installed, FALSE if it was removed
 */
void appOnDownloadableContentChanged(UBOOL bContentWasInstalled)
{
	// BmGame doesn't care
}

FSystemSettings& GetSystemSettings()
{
	static FSystemSettings SystemSettingsSingleton;
	return SystemSettingsSingleton;
}
