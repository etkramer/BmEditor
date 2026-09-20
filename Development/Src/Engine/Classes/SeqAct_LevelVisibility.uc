/**
 * SeqAct_LevelVisibility
 *
 * Kismet action exposing associating/ dissociating of levels.
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_LevelVisibility extends SeqAct_Latent
	native(Sequence);

/** LevelStreaming object that is going to be associated/ dissociated on request */
var transient LevelStreaming Level;

/** LevelStreaming object name */
var() Name LevelName<autocomment=true>;

// BM
/** Additional levels acted on alongside LevelName, and their resolved LevelStreaming objects */
var() array<Name> Levels;
var transient array<LevelStreaming> CachedLevels;

var transient bool bStatusIsOk;

// BM
var transient bool bHidingLevels;
var transient bool bSetLevelUnhidden;

cpptext
{
	void Activated();
	UBOOL UpdateOp(FLOAT DeltaTime);
	virtual void DrawExtraInfo(FCanvas* Canvas, const FVector& BoxCenter);
	virtual void UpdateStatus();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
};

defaultproperties
{
	ObjName="Change Level Visibility"
	ObjCategory="Level"
	VariableLinks.Empty
	OutputLinks.Empty
	InputLinks(0)=(LinkDesc="Make Visible")
	InputLinks(1)=(LinkDesc="Hide")
	OutputLinks(0)=(LinkDesc="Finished")
}
