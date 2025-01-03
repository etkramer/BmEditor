/*=============================================================================
	UE3AppDelegate.mm: Mac application class and main loop.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#import "UE3AppDelegate.h"
#include "MacObjCWrapper.h"

extern int appMacCallGuardedMain();
extern uint32_t GIsStarted;
extern int GToggleFullscreen;
extern void appExit();

@implementation UE3AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	NSString *path = [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent];
	if ([[path lastPathComponent] isEqual:@"Mac"])
	{
		chdir([[NSFileManager defaultManager] fileSystemRepresentationWithPath:path]);
	}
	else if ( [[path lastPathComponent] isEqual:@"Debug"] || [[path lastPathComponent] isEqual:@"Release"] )
	{
		chdir([[NSFileManager defaultManager] fileSystemRepresentationWithPath:[path stringByAppendingPathComponent:@".."]]);
	}
	else
	{
		chdir([[NSFileManager defaultManager] fileSystemRepresentationWithPath:[path stringByAppendingPathComponent:@"Engine/Config"]]);
	}

	// We will be using pthreads, nevertheless Cocoa needs to know it must run in
	// multithreaded mode. To do this we simply create Cocoa thread that will exit immediately.
	[NSThread detachNewThreadSelector:@selector(DummyThreadRoutine:) toTarget:[UE3AppDelegate class] withObject:nil];

	GIsStarted = 1;

	appMacCallGuardedMain();

	appExit();

	GIsStarted = 0;
	
	exit(0);
}

+ (void)DummyThreadRoutine:(id)AnObject
{
}

- (IBAction)toggleFullScreen:(id)Sender
{
	GToggleFullscreen = 1;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)Sender
{
	MacAppRequestExit( 0 );
	return NSTerminateCancel;
}

@end
