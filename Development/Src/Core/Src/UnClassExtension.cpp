/*=============================================================================
	UnClassExtension.cpp: Native implementations for script classes loaded from disk.
	See UnClassExtension.h for the rationale and the no-data-members rule.
=============================================================================*/

#include "CorePrivate.h"

#if BATMAN

/** Head of the registration list. Extensions link themselves in at static init. */
static FClassExtension* GClassExtensions = NULL;

FClassExtension::FClassExtension( const TCHAR* InClassPathName, void (*InClassConstructor)(void*), INT InClassSize )
:	ClassPathName( InClassPathName )
,	ClassConstructor( InClassConstructor )
,	ClassSize( InClassSize )
,	NextExtension( GClassExtensions )
{
	GClassExtensions = this;
}

FClassExtension* FindClassExtension( UClass* Cls )
{
	// Only ever reached for classes without a C++ counterpart, and only from Bind.
	if( !GClassExtensions || !Cls )
	{
		return NULL;
	}

	const FString PathName = Cls->GetPathName();

	for( FClassExtension* Extension=GClassExtensions; Extension; Extension=Extension->NextExtension )
	{
		if( PathName == Extension->ClassPathName )
		{
			return Extension;
		}
	}

	return NULL;
}

void RegisterExtensionNatives( const TCHAR* ClassName, FNativeFunctionLookup* Table )
{
	GNativeLookupFuncs.Set(FName(ClassName), Table);
}

#endif // BATMAN
