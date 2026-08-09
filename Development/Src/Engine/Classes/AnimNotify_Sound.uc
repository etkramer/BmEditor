/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class AnimNotify_Sound extends AnimNotify
	native(Anim);

struct native SoundCharacterFilter
{
	var() bool Batman;
	var() bool Robin;
	var() bool Nightwing;
	var() bool Catwoman;
	var() bool Ras;
	var() bool Ninja;
	var() bool BruceWayne;
	var() bool TygerGuard;
	var() bool Thug;
};

var()	AkEvent					EventName;
var()	bool					bFollowActor;
var()	bool					bIgnoreIfActorHidden;
var()	editoronly bool			bTempMute;
var		bool					CharacterFilter_Enabled;
var()	name					BoneName;
var()	SoundCharacterFilter	CharacterFilter;

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );

	virtual FString GetEditorComment() { return TEXT("Snd"); }
}

defaultproperties
{
	bFollowActor=TRUE
}
