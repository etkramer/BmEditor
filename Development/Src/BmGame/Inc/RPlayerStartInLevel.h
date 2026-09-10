/*=============================================================================
	RPlayerStartInLevel.h
	BM: Native implementation for BM2's RPlayerStartInLevel.
=============================================================================*/

#ifndef RPLAYERSTARTINLEVEL_H
#define RPLAYERSTARTINLEVEL_H

// Declares no data members - see UnClassExtension.h.
class ARPlayerStartInLevel : public APlayerStart
{
public:
	// DECLARE_CLASS would normally provide this; we have no UClass of our own.
	typedef APlayerStart Super;

	virtual void PostEditMove(UBOOL bFinished);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void CheckForErrors();

	/** Stamps Level and the streaming level lists from whichever RLevelVolume encloses us. */
	void UpdateConnectingLevel();

	/** Resolve the script properties we touch. FALSE if the class isn't what we expect. */
	UBOOL BindProperties();

private:
	FName& Level()									{ return LevelProp(this); }
	TArray<FName>& AdditionalStreamingLevels()		{ return AdditionalStreamingLevelsProp(this); }
	TArray<FName>& AdditionalStreamingLODLevels()	{ return AdditionalStreamingLODLevelsProp(this); }

	static TExtensionProperty<FName>			LevelProp;
	static TExtensionProperty<TArray<FName> >	AdditionalStreamingLevelsProp;
	static TExtensionProperty<TArray<FName> >	AdditionalStreamingLODLevelsProp;
};

#endif // RPLAYERSTARTINLEVEL_H
