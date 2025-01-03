/*=============================================================================
	UE3AppDelegate.h: Mac application class and main loop.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#import <Cocoa/Cocoa.h>

@interface UE3AppDelegate : NSObject <NSApplicationDelegate>
{
}

+ (void)DummyThreadRoutine:(id)AnObject;
- (IBAction)toggleFullScreen:(id)Sender;

@end
