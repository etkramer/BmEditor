/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class RGameInfo extends GameInfo;

auto State PendingMatch
{
Begin:
	StartMatch();
}

defaultproperties
{
	HUDType=class'GameFramework.MobileHUD'
	PlayerControllerClass=class'BmGame.RPlayerController'
	DefaultPawnClass=class'BmGame.RPawn'
	bDelayedStart=false
}


