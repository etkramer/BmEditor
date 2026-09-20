/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_AttachToActor extends SequenceAction;

/** if true, then attachments will be detached. */
var() bool		bDetach;

/** Should hard attach to the actor */
var() bool		bHardAttach;

/** true if attachment should be set relatively to the target, using an offset */
var() bool		bUseRelativeOffset;

/** Use relative rotation offset */
var() bool		bUseRelativeRotation;

// BM
var() bool		bIgnoreBaseRotation;
var() bool		bUseTickGroup;
var() bool		bDropOnCombatSp;
var() bool		bAttachToWeapon;
var() bool		bAttachToLeftProp;
var() bool		bAttachToRightProp;
var() bool		bAttemptToInheritLightEnvironment;

/** Bone Name to use for attachment */
var() Name		BoneName;

/** offset to use when attaching */
var() vector	RelativeOffset;

/** relative rotation */
var()	Rotator	RelativeRotation;

// BM
/** Tick group forced on the attachment when bUseTickGroup is set */
var() ETickingGroup	TickGroup;

/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Attach to Actor"
	ObjCategory="Actor"

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Attachment")

	bHardAttach=TRUE
}
