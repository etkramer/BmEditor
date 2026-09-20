/**
 * Dumps the runtime property layout of every loaded class and script struct, so our offsets can be
 * diffed against the ones baked into cooked packages.
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class DumpClassLayoutCommandlet extends Commandlet
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
	 * @return	0 if the commandlet succeeded; otherwise, an error code defined by the commandlet.
	 */
	virtual INT Main(const FString& Params);
}

DefaultProperties
{
}
