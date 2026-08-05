/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
//=============================================================================
// RAdditionalContentFactoryNew
//=============================================================================

class RAdditionalContentFactoryNew
	extends Factory
	native
	hidecategories(Object);

// BM: BM2 declares this as class<RAdditionalContent>, but that class lives in the cooked
// packages, so the metaclass is enforced in FactoryCreateNew instead.
var() class ContentType;

var const string SupportedClassName;

cpptext
{
	static void FixupContentTypeMetaClass();

	virtual UObject* FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn );
}

defaultproperties
{
	Description="Additional Content"
	SupportedClassName="BmGame.RAdditionalContent"
	bCreateNew=true
}
