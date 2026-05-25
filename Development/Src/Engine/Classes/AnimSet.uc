/**
 * This is a set of AnimSequences
 * All sequence have the same number of tracks, and they relate to the same bone names.
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

class AnimSet extends Object
	native(Anim)
	dependson(RAnimZip_Settings)
	hidecategories(Object);


/** This is a mapping table between each bone in a particular skeletal mesh and the tracks of this animation set. */
struct native AnimSetMeshLinkup
{
	/**
	 * Mapping table. Size must be same as size of SkelMesh reference skeleton.
	 * No index should be more than the number of tracks in this AnimSet.
	 * -1 indicates no track for this bone - will use reference pose instead.
	 */
	var array<INT> BoneToTrackTable;
	var array<INT> TrackToBoneTable;

	structcpptext
	{
		/** Reset this linkup and re-create between the provided skeletal mesh and anim set. */
		void BuildLinkup(USkeletalMesh* InSkelMesh, UAnimSet* InAnimSet);
	}
};

/** BM: Preview partner descriptor used by the AnimSet editor. */
struct native AnimSetPreviewPartner
{
	var() name		SkelMeshName;
	var() name		ExtraSkelMesh1Name;
	var() name		AnimSetName;
	var() string	AnimPostfixName;
};

var transient bool			bAnimRotationOnly;
/** BM: When set, idle configs reference the full AnimSet rather than a filtered subset. */
var() bool					bReferenceFullAnimSetInIdleConfigs;
/** BM: Strip face tracks on cook. */
var(AutoDeleteTracks) bool	AutoDeleteTracks_Face;
/** BM: Strip eye tracks on cook. */
var(AutoDeleteTracks) bool	AutoDeleteTracks_Eyes;
/** BM: Strip nub tracks on cook. */
var(AutoDeleteTracks) bool	AutoDeleteTracks_Nubs;
/** BM: Strip flappy tracks on cook. */
var(AutoDeleteTracks) bool	AutoDeleteTracks_Flappy;
/** BM: Override per-sequence compression settings with the AnimSet's. */
var(Compression) bool		Compression_OverrideIndividualAnimSettings;

/** Bone name that each track relates to. TrackBoneName.Num() == Number of tracks. */
var array<name>				TrackBoneNames;

/** Actual animation sequence information. */
var	array<AnimSequence>		Sequences;
/** Lookup-cache, populated in PostLoad. */
var	native transient Map{FName,INT} SequenceCache;

/** Non-serialised cache of linkups between different skeletal meshes and this AnimSet. */
var transient array<AnimSetMeshLinkup>	LinkupCache;
/** Runtime built mapping table between SkeletalMeshes, and LinkupCache array indices. */
var native transient Map{FName,INT} SkelMesh2LinkupCache;

/** In the AnimSetEditor, when you switch to this AnimSet, it sees if this skeletal mesh is loaded and if so switches to it. */
var()	name				PreviewSkelMeshName;
var()	nontransactional name	PreviewExtraSkelMesh1Name;
/** BM: Additional preview mesh slot. */
var()	nontransactional name	PreviewExtraSkelMesh2Name;
/** BM: Additional preview mesh slot. */
var()	nontransactional name	PreviewExtraSkelMesh3Name;
/** Holds the name of the skeletal mesh whose reference skeleton best matches the TrackBoneName array. */
var		name				BestRatioSkelMeshName;
/** BM: List of preview partner descriptors for the AnimSet editor. */
var()	editoronly array<AnimSetPreviewPartner>	PreviewPartners2;
/** BM: Original path of the AnimSet at cook time. */
var		string				PreCookingPathName;
/** BM: AnimZip compression preset for sequences in this set. */
var(Compression) RAnimZip_Settings.EAnimZipPreset	Compression_Preset;
/**
 * Per-anim-set AnimZip encoder overrides. When NULL, the encoder falls back to
 * the RAnimZip_Settings CDO.
 */
var(Compression) editinline editoronly RAnimZip_Settings Compression_CustomSettings;

cpptext
{
	// UObject interface
	virtual void PostLoad();
	virtual void BeginDestroy();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// UAnimSet interface
	/**
	 * See if we can play sequences from this AnimSet on the provided SkeletalMesh.
	 * Returns true if there is a bone in SkelMesh for every track in the AnimSet,
	 * or there is a track of animation for every bone of the SkelMesh.
	 *
	 * @param	SkelMesh	SkeletalMesh to compare the AnimSet against.
	 * @return				TRUE if animation set can play on supplied SkeletalMesh, FALSE if not.
	 */
	UBOOL CanPlayOnSkeletalMesh(USkeletalMesh* SkelMesh) const;

	/** Get Ratio of how much that mesh fits that animation set */
	FLOAT GetSkeletalMeshMatchRatio(USkeletalMesh* SkelMesh) const;

	/**
	 * Returns the AnimSequence with the specified name in this set.
	 *
	 * @param		SequenceName	Name of sequence to find.
	 * @return						Pointer to AnimSequence with desired name, or NULL if sequence was not found.
	 */
	UAnimSequence* FindAnimSequence(FName SequenceName);

	/**
	 * Find a mesh linkup table (mapping of sequence tracks to bone indices) for a particular SkeletalMesh
	 * If one does not already exist, create it now.
	 */
	INT GetMeshLinkupIndex(USkeletalMesh* SkelMesh);

	/**
	 * @return		The track index for the bone with the supplied name, or INDEX_NONE if no track exists for that bone.
	 */
	INT FindTrackWithName(FName BoneName) const
	{
		return TrackBoneNames.FindItemIndex( BoneName );
	}

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	INT GetResourceSize();

	/**
	 * Clears all sequences and resets the TrackBoneNames table.
	 */
	void ResetAnimSet();
	/**
	 * Properly remove an AnimSequence from an AnimSet, and updates references it might have.
	 * @return TRUE if AnimSequence was properly removed, FALSE if it wasn't found.
	 */
	UBOOL RemoveAnimSequenceFromAnimSet(UAnimSequence* AnimSeq);

	/** Util that find all AnimSets and flushes their LinkupCache, then calls InitAnimTree on all SkeletalMeshComponents. */
	static void ClearAllAnimSetLinkupCaches();

	/**
	 * Animation Usage Tracking
	 */
	void	TraceAnimationUsage();
	void	RecordAnimationUsage();

	static void OutputAnimationUsage();
	static void CleanUpAnimationUsage();
	static void TickAnimationUsage();
}

defaultproperties
{
	AutoDeleteTracks_Face=true
	AutoDeleteTracks_Eyes=true
	AutoDeleteTracks_Nubs=true
}
