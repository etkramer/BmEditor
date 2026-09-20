/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_GetDistance extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
}

// BM
/** Ignore the Z axis when measuring */
var() bool bGet2DDistance;

var() editconst float Distance;

defaultproperties
{
	ObjName="Get Distance"
	ObjCategory="Actor"

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="A")
	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="B")
	VariableLinks(2)=(ExpectedType=class'SeqVar_Float',LinkDesc="Distance",bWriteable=true,PropertyName=Distance)
}
