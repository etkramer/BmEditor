/**
 * Loads packages headlessly and reports anything that went wrong while deserializing them, including the
 * load diagnostics BATMAN downgrades from errors to warnings.
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class CheckPackageLoadCommandlet extends Commandlet
	native;

cpptext
{
	/**
	 * Creates a bare editor engine so the commandlet can run without the editor's content dependencies.
	 */
	virtual void CreateCustomEngine();

	/**
	 * Commandlet entry point
	 *
	 * @param	Params	the command line parameters that were passed in.
	 *
	 * @return	0 if every package loaded cleanly; otherwise, the number of packages that failed.
	 */
	virtual INT Main(const FString& Params);
}

DefaultProperties
{
}
