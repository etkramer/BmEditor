/*=============================================================================
	UnClassExtension.h: Native implementations for script classes loaded from disk.
	Lets us attach C++ behaviour to classes that already exist in the game's
	script packages (RHidePoint, RSkeletalMeshActor, ...) without redeclaring
	them - and therefore without dragging in their dependency graph.

	Such classes have no C++ counterpart, so UClass::Bind would chase the
	constructor down to their nearest bound ancestor. IMPLEMENT_CLASS_EXTENSION
	registers a constructor against a class path name instead, so instances are
	built with our vtable and ordinary virtual overrides just work.

	The class must declare NO data members - property offsets come from the
	loaded UClass and are load-bearing for serialization, so the C++ side has to
	stay layout-neutral. UClass::Link enforces this against PropertiesSize.
	Use TExtensionProperty for typed access to the script properties.

	Native script functions need no registration beyond a lookup table, since
	UFunction::Bind resolves them by name (see UnClass.cpp). Name the C++ class
	A<ScriptClassName> and MAP_NATIVE lines up with what it searches for.
=============================================================================*/

#ifndef UNCLASSEXTENSION_H
#define UNCLASSEXTENSION_H

#if BATMAN

/*-----------------------------------------------------------------------------
	Class extensions.
-----------------------------------------------------------------------------*/

/** A C++ implementation registered against a script class loaded from disk. */
struct FClassExtension
{
	FClassExtension( const TCHAR* InClassPathName, void (*InClassConstructor)(void*), INT InClassSize );

	const TCHAR*	ClassPathName;
	void			(*ClassConstructor)(void*);
	INT				ClassSize;

	/** Registration list, walked by UClass::Bind. */
	FClassExtension* NextExtension;
};

/** Returns the extension registered for a class, or NULL. Called from UClass::Bind. */
extern FClassExtension* FindClassExtension( UClass* Cls );

/**
 * Registers TClass as the native implementation of a script class.
 * ClassPathName is a string literal, e.g. "BmGame.RHidePoint".
 */
#define IMPLEMENT_CLASS_EXTENSION( TClass, ClassPathName ) \
	static void TClass##ExtensionConstructor( void* X ) { new( (EInternal*)X )TClass; } \
	static FClassExtension TClass##ClassExtension( TEXT(ClassPathName), &TClass##ExtensionConstructor, sizeof(TClass) );

/** Registers a native function lookup table against a script class name. */
extern void RegisterExtensionNatives( const TCHAR* ClassName, FNativeFunctionLookup* Table );

/*-----------------------------------------------------------------------------
	Property access.
-----------------------------------------------------------------------------*/

/**
 * A property offset resolved by name against a loaded UClass, so nothing about
 * the class layout is baked into C++. Bind() validates element size and array
 * dimension, failing loudly instead of corrupting memory at access time.
 */
template<typename T>
struct TExtensionProperty
{
	TExtensionProperty()
	:	Offset(INDEX_NONE)
	,	ArrayDim(0)
	{}

	UBOOL Bind( UClass* Cls, const TCHAR* PropertyName, INT ExpectedArrayDim=1 )
	{
		if (IsBound())
		{
			return TRUE;
		}

		Offset = INDEX_NONE;
		ArrayDim = 0;

		UProperty* Prop = FindField<UProperty>(Cls, PropertyName);
		if( !Prop )
		{
			warnf(NAME_Warning, TEXT("ClassExtension: %s has no property '%s'"), *Cls->GetName(), PropertyName);
			return FALSE;
		}

		if( Prop->ElementSize != sizeof(T) )
		{
			warnf(NAME_Warning, TEXT("ClassExtension: %s.%s elements are %i bytes, expected %i"),
				*Cls->GetName(), PropertyName, Prop->ElementSize, (INT)sizeof(T));
			return FALSE;
		}

		if( Prop->ArrayDim != ExpectedArrayDim )
		{
			warnf(NAME_Warning, TEXT("ClassExtension: %s.%s has ArrayDim %i, expected %i"),
				*Cls->GetName(), PropertyName, Prop->ArrayDim, ExpectedArrayDim);
			return FALSE;
		}

		Offset = Prop->Offset;
		ArrayDim = Prop->ArrayDim;
		return TRUE;
	}

	UBOOL IsBound() const
	{
		return Offset != INDEX_NONE;
	}

	T& operator()( UObject* Obj, INT Index=0 ) const
	{
		checkSlow(IsBound());
		checkSlow(Index >= 0 && Index < ArrayDim);
		return *(T*)((BYTE*)Obj + Offset + Index * sizeof(T));
	}

	INT Offset;
	INT ArrayDim;
};

#endif // BATMAN

#endif // UNCLASSEXTENSION_H
