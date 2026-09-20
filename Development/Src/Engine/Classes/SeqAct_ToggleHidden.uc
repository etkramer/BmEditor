/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ToggleHidden extends SeqAct_Toggle;

// BM
var() bool bToggleCollision;
// BM
var() bool bDontAffectStaticMeshToggleableCollision;
var() bool bToggleBasedActors;
var() array< class<Actor> > IgnoreBasedClasses;


defaultproperties
{
	ObjName="Toggle Hidden"
	ObjCategory="Toggle"

	InputLinks(0)=(LinkDesc="Hide")
	InputLinks(1)=(LinkDesc="UnHide")
}
