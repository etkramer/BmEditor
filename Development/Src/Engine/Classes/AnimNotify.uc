/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class AnimNotify extends Object
	native(Anim)
	abstract
	editinlinenew
	hidecategories(Object)
	collapsecategories;

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq ) {}
	virtual void NotifyTick( class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime, FLOAT AnimTimeStep, FLOAT InTotalDuration ) {}
	virtual void NotifyEnd( class UAnimNodeSequence* NodeSeq, FLOAT AnimCurrentTime ) {}

	virtual FString GetEditorComment() { return TEXT(""); }
	virtual FColor GetEditorColor() { return FColor(255,200,200,255); }
}

simulated function bool FindNextNotifyOfClass(AnimNodeSequence AnimSeqInstigator, class<AnimNotify> NotifyClass, out AnimNotifyEvent OutEvent)
{
	local AnimSequence Seq;
	local int i;
	local bool bFoundThis;

	if(AnimSeqInstigator.AnimSeq != None)
	{
		// we look through the notifies to find the end that corresponds to this start
		Seq = AnimSeqInstigator.AnimSeq;
		for(i=0; i<Seq.Notifies.length; i++)
		{
			// Found us - remember the time
			if(Seq.Notifies[i].Notify == self)
			{
				bFoundThis = TRUE;
			}

			// First notify of desired class after this 'start'
			if(bFoundThis && ClassIsChildOf(Seq.Notifies[i].Notify.Class, NotifyClass))
			{
				// Copy info from event
				OutEvent = Seq.Notifies[i];
				// and set bool
				return TRUE;
			}
		}
	}
	
	return false;
}
