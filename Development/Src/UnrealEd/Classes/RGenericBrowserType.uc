/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// RGenericBrowserType: base browser type for BM2 config assets
//=============================================================================

class RGenericBrowserType
	extends GenericBrowserType
	abstract
	native;

var const class SupportedClass;
var const color BorderColor;

// BM: BM2 sets SupportedClass in defaults, but our BM2 classes come from the cooked
// packages rather than a compiled script package, so allow resolving by path instead.
var const string SupportedClassName;

cpptext
{
	virtual void Init();
	virtual UBOOL ShowObjectEditor( UObject* InObject );
}
