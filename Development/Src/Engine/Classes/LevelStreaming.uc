/**
 * LevelStreaming
 *
 * Abstract base class of container object encapsulating data required for streaming and providing 
 * interface for when a level should be streamed in and out of memory.
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class LevelStreaming extends Object
	abstract
	editinlinenew
	native;

enum ELightingGroup
{
	LightingGroup_All,
	LightingGroup_1,
	LightingGroup_2,
	LightingGroup_3,
	LightingGroup_4,
	LightingGroup_5,
	LightingGroup_6,
	LightingGroup_7,
	LightingGroup_8,
	LightingGroup_9,
	LightingGroup,
	LightingGroup_MAX
};

enum ENavMeshGroup
{
	NavMeshGroup_All,
	NavMeshGroup_1,
	NavMeshGroup_1a,
	NavMeshGroup_1b,
	NavMeshGroup_1c,
	NavMeshGroup_2,
	NavMeshGroup_2a,
	NavMeshGroup_2b,
	NavMeshGroup_2c,
	NavMeshGroup_3,
	NavMeshGroup_3a,
	NavMeshGroup_3b,
	NavMeshGroup_3c,
	NavMeshGroup_4,
	NavMeshGroup_4a,
	NavMeshGroup_4b,
	NavMeshGroup_4c,
	NavMeshGroup_5,
	NavMeshGroup_5a,
	NavMeshGroup_5b,
	NavMeshGroup_5c,
	NavMeshGroup_6,
	NavMeshGroup_6a,
	NavMeshGroup_6b,
	NavMeshGroup_6c,
	NavMeshGroup_7,
	NavMeshGroup_7a,
	NavMeshGroup_7b,
	NavMeshGroup_7c,
	NavMeshGroup_8,
	NavMeshGroup_8a,
	NavMeshGroup_8b,
	NavMeshGroup_8c,
	NavMeshGroup_9,
	NavMeshGroup,
	NavMeshGroup_MAX
};

/** Name of the level package name used for loading.																		*/
var() editconst const name							PackageName;

/** Package name as a string, as cooked by BM2.																			*/
var string											PackageNameAsString;

/** Pointer to Level object if currently loaded/ streamed in.																*/
var transient const	level							LoadedLevel;

/** Offset applied to actors after loading.																					*/
var() const			vector							Offset;

/** Current/ old offset required for changing the offset at runtime, e.g. in the Editor.									*/
var const			vector							OldOffset;	

/** Whether the level is currently visible/ associated with the world														*/
var const transient bool							bIsVisible;

/** Whether the level is hidden.																							*/
var const transient bool							bIsLevelHidden;

/** Whether we currently have a load request pending.																		*/
var const transient	bool							bHasLoadRequestPending;

/** Whether we currently have an unload request pending.																	*/
var const transient bool							bHasUnloadRequestPending;

/** Whether this level should be visible in the Editor																		*/
var() const			bool							bShouldBeVisibleInEditor;

/** Whether this level's bounding box should be visible in the Editor.														*/
var const			bool							bBoundingBoxVisible;

/** Whether this level is locked; that is, its actors are read-only.														*/
var() const			bool							bLocked;

/** Whether this level is fully static - if it is, then assumptions can be made about it, ie it doesn't need to be reloaded since nothing could have changed */
var() const			bool							bIsFullyStatic;

/** Whether this level should be transferred to the next persistent map.													*/
var() const			bool							bTransferToNextPersistentMap;

/** Whether the level should be loaded																						*/
var	const transient bool bShouldBeLoaded;

/** Whether the level should be visible if it is loaded																		*/
var const transient bool bShouldBeVisible;

/** Whether the level should be hidden.																					*/
var transient		bool							bShouldBeLevelHidden;

/** Whether we want to force a blocking load																				*/
var transient		bool							bShouldBlockOnLoad;

/** Whether this level should be visible in the level browser.																*/
var transient		bool							bShouldBeVisibleInLevelBrowser;

/** Whether this level is expanded in the level browser.																	*/
var transient		bool							bExpandedInLevelBrowser;

/** Whether this level is a parent in the level browser.																	*/
var transient		bool							bIsParent;

/** Whether this level is a child in the level browser.																		*/
var transient		bool							bIsChild;

/** If TRUE, will be drawn on the 'level streaming status' map (STAT LEVELMAP console command) */
var()				bool							bDrawOnLevelStatusMap;

/** Whether this level streaming object's level should be unloaded and the object be removed from the level list.			*/
var const transient bool							bIsRequestingUnloadAndRemoval;

/** Whether this level is excluded from path building.																		*/
var					bool							bExcludeFromPathBuilding;

/** Previous path-building visibility state.																				*/
var transient		bool							bExcludeFromPathBuilding_Old_bShouldBeVisibleInEditor;

/** Whether shadow volumes should be forced for this level.																*/
var					bool							ForceShadowVolumes;

/** Whether this level has a high-priority load request.																	*/
var transient		bool							bHighPriorityLoadRequest;

/** Whether this is a chapter level.																						*/
var					bool							bIsChapterLevel;

/** The level's color; used to make the level easily identifiable in the level browser, for actor level visulization, etc.	*/
var() const			color							DrawColor;

/** The level streaming volumes bound to this level.																		*/
var() const editconst array<LevelStreamingVolume>	EditorStreamingVolumes;

/** Cooldown time in seconds between volume-based unload requests.  Used in preventing spurious unload requests.			*/
var() float											MinTimeBetweenVolumeUnloadRequests;

/** Time of last volume unload request.  Used in preventing spurious unload requests.										*/
var const transient float							LastVolumeUnloadRequestTime;

/** Person responsible for this level.																					*/
var string											PersonResponsible;

/** BM2 lighting group for this streaming level.																			*/
var ELightingGroup									LightingGroup;

/** BM2 navigation mesh group for this streaming level.																		*/
var ENavMeshGroup									NavMeshGroup;

/** List of keywords to filter on in the level browser */
var array<string> Keywords;

/** The grid volume bound to this level, if any */
var() const editconst LevelGridVolume EditorGridVolume;

/** Row, column and depth of this streaming level in a streaming grid network */
var() const editconst int GridPosition[ 3 ];


cpptext
{
	/**
	 * Returns whether this level should be present in memory which in turn tells the 
	 * streaming code to stream it in. Please note that a change in value from FALSE 
	 * to TRUE only tells the streaming code that it needs to START streaming it in 
	 * so the code needs to return TRUE an appropriate amount of time before it is 
	 * needed.
	 *
	 * @param ViewLocation	Location of the viewer
	 * @return TRUE if level should be loaded/ streamed in, FALSE otherwise
	 */
	virtual UBOOL ShouldBeLoaded( const FVector& ViewLocation );

	/**
	 * Returns whether this level should be visible/ associated with the world if it is
	 * loaded.
	 * 
	 * @param ViewLocation	Location of the viewer
	 * @return TRUE if the level should be visible, FALSE otherwise
	 */
	virtual UBOOL ShouldBeVisible( const FVector& ViewLocation );
	
	/** Get a bounding box around the streaming volumes associated with this LevelStreaming object */
	FBox GetStreamingVolumeBounds();

	// UObject interface.
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent);
}

defaultproperties
{
	bShouldBeVisibleInEditor=TRUE
	DrawColor=(R=255,G=255,B=255,A=255)

	MinTimeBetweenVolumeUnloadRequests=2.0

	bDrawOnLevelStatusMap=TRUE
}
