/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"

IMPLEMENT_CLASS(UOnlinePlaylistManager);

/**
 * Uses the configuration data to create the requested objects and then applies any
 * specific game settings changes to them
 */
void UOnlinePlaylistManager::FinalizePlaylistObjects(void)
{
	// Process the config entries creating and updating as specified
	for (INT PlaylistIndex = 0; PlaylistIndex < Playlists.Num(); PlaylistIndex++)
	{
		FPlaylist& Playlist = Playlists(PlaylistIndex);
		// Create each game setting object and update its data
		for (INT GameIndex = 0; GameIndex < Playlist.ConfiguredGames.Num(); GameIndex++)
		{
			FConfiguredGameSetting& ConfiguredGame = Playlist.ConfiguredGames(GameIndex);
			// If there is a valid class name specified try to load it and instance it
			if (ConfiguredGame.GameSettingsClassName.Len())
			{
				UClass* GameSettingsClass = LoadClass<UOnlineGameSettings>(NULL,
					*ConfiguredGame.GameSettingsClassName,
					NULL,
					LOAD_None,
					NULL);
				if (GameSettingsClass)
				{
					// Now create an instance with that class
					ConfiguredGame.GameSettings = ConstructObject<UOnlineGameSettings>(GameSettingsClass);
					if (ConfiguredGame.GameSettings)
					{
						// Update the game object with these settings, if not using the defaults
						if (ConfiguredGame.URL.Len())
						{
							ConfiguredGame.GameSettings->UpdateFromURL(ConfiguredGame.URL,NULL);
						}
					}
					else
					{
						debugf(NAME_DevOnline,
							TEXT("Failed to create class (%s) for playlist (%s)"),
							*ConfiguredGame.GameSettingsClassName,
							*Playlist.Name);
					}
				}
				else
				{
					debugf(NAME_DevOnline,
						TEXT("Failed to load class (%s) for playlist (%s)"),
						*ConfiguredGame.GameSettingsClassName,
						*Playlist.Name);
				}
			}
		}
	}
	if (DatastoresToRefresh.Num())
	{
		INT DatastoreIndex = INDEX_NONE;
		// Iterate through the registered set of datastores and refresh them
		for (TObjectIterator<UUIDataStore_GameResource> ObjIt; ObjIt; ++ObjIt)
		{
			DatastoresToRefresh.FindItem(ObjIt->Tag,DatastoreIndex);
			// Don't refresh it if it isn't in our list
			if (DatastoreIndex != INDEX_NONE)
			{
				(*ObjIt)->InitializeListElementProviders();
			}
		}
	}
}

/** Uses the current loc setting and game ini name to build the download list */
void UOnlinePlaylistManager::DetermineFilesToDownload(void)
{
	PlaylistFileNames.Empty(4);
	// Build the game specific playlist ini
	PlaylistFileNames.AddItem(FString::Printf(TEXT("%sPlaylist.ini"),appGetGameName()));
	FFilename GameIni(GGameIni);
	// Add the game ini for downloading per object config
	PlaylistFileNames.AddItem(GameIni.GetCleanFilename());
	// Now build the loc file name from the ini filename
	PlaylistFileNames.AddItem(FString::Printf(TEXT("Engine.%s"),*appGetLanguageExt()));
	PlaylistFileNames.AddItem(FString::Printf(TEXT("%sGame.%s"),appGetGameName(),*appGetLanguageExt()));
}

/**
 * Converts the data into the structure used by the playlist manager
 *
 * @param Data the data that was downloaded
 */
void UOnlinePlaylistManager::ParsePlaylistPopulationData(const TArray<BYTE>& Data)
{
	// Make sure the string is null terminated
	((TArray<BYTE>&)Data).AddItem(0);
	// Convert to a string that we can work with
	FString StrData = ANSI_TO_TCHAR((ANSICHAR*)Data.GetTypedData());
	TArray<FString> Lines;
	// Now split into lines
	StrData.ParseIntoArray(&Lines,TEXT("\r\n"),TRUE);
	FString Token(TEXT("="));
	FString Right;
	// Mimic how the config cache stores them by removing the PopulationData= from the entries
	for (INT Index = 0; Index < Lines.Num(); Index++)
	{
		if (Lines(Index).Split(Token,NULL,&Right))
		{
			Lines(Index) = Right;
		}
	}
	// If there was data in the file, then import that into the array
	if (Lines.Num() > 0)
	{
		// Find the property on our object
		UArrayProperty* Array = FindField<UArrayProperty>(GetClass(),FName(TEXT("PopulationData")));
		if (Array != NULL)
		{
			const INT Size = Array->Inner->ElementSize;
			FScriptArray* ArrayPtr = (FScriptArray*)((BYTE*)this + Array->Offset);
			// Destroy any data that was already there
			Array->DestroyValue(ArrayPtr);
			// Size everything to the number of lines that were downloaded
			ArrayPtr->AddZeroed(Lines.Num(),Size);
			// Import each line of the population data
			for (INT ArrayIndex = Lines.Num() - 1, Count = 0; ArrayIndex >= 0; ArrayIndex--, Count++)
			{
				Array->Inner->ImportText(*Lines(ArrayIndex),(BYTE*)ArrayPtr->GetData() + Count * Size,PPF_ConfigOnly,this);
			}
		}
	}
	WorldwideTotalPlayers = RegionTotalPlayers = 0;
	// Total up the worldwide and region data
	for (INT DataIndex = 0; DataIndex < PopulationData.Num(); DataIndex++)
	{
		const FPlaylistPopulation& Data = PopulationData(DataIndex);
		WorldwideTotalPlayers += Data.WorldwideTotal;
		RegionTotalPlayers += Data.RegionTotal;
	}
}

/**
 * Determines whether an update of the playlist population information is needed or not
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlinePlaylistManager::Tick(FLOAT DeltaTime)
{
	UBOOL bNeedsAnUpdate = FALSE;
	// Determine if we've passed our update window and mark that we need an update
	// NOTE: to handle starting a match, reporting, and quiting, the code has to always
	// tick the interval so we don't over/under report
	NextPlaylistPopulationUpdateTime += DeltaTime;
	if (NextPlaylistPopulationUpdateTime >= PlaylistPopulationUpdateInterval &&
		PlaylistPopulationUpdateInterval > 0.f)
	{
		bNeedsAnUpdate = TRUE;
		NextPlaylistPopulationUpdateTime = 0.f;
	}
	// We can only update if we are the server
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	if (WorldInfo != NULL &&
		WorldInfo->NetMode != NM_Standalone &&
		WorldInfo->NetMode != NM_Client &&
		// Don't send updates when we aren't playing a playlist
		CurrentPlaylistId > MinPlaylistIdToReport)
	{
		if (bNeedsAnUpdate)
		{
			INT NumPlayers = 0;
			// Work through the controller list counting players and skipping bots
			for (AController* Controller = WorldInfo->ControllerList; Controller; Controller = Controller->NextController)
			{
				APlayerController* PC = Cast<APlayerController>(Controller);
				if (PC)
				{
					NumPlayers++;
				}
			}
			eventSendPlaylistPopulationUpdate(NumPlayers);
		}
	}
}


/**
 * Converts the data into the datacenter id
 *
 * @param Data the data that was downloaded
 */
void UOnlinePlaylistManager::ParseDataCenterId(const TArray<BYTE>& Data)
{
	// Make sure the string is null terminated
	((TArray<BYTE>&)Data).AddItem(0);
	// Convert to a string that we can work with
	const FString StrData = ANSI_TO_TCHAR((ANSICHAR*)Data.GetTypedData());
	// Find the property on our object
	UIntProperty* Property = FindField<UIntProperty>(GetClass(),FName(TEXT("DataCenterId")));
	if (Property != NULL)
	{
		// Import the text sent to us by the network layer
		if (Property->ImportText(*StrData,(BYTE*)this + Property->Offset,PPF_ConfigOnly,this) == NULL)
		{
			debugf(NAME_Error,
				TEXT("LoadConfig (%s): import failed for %s in: %s"),
				*GetPathName(),
				*Property->GetName(),
				*StrData);
		}
	}
}

IMPLEMENT_CLASS(UOnlinePlaylistProvider);
IMPLEMENT_CLASS(UUIDataStore_OnlinePlaylists);

/* === UIDataStore interface === */	
/**
 * Loads the classes referenced by the ElementProviderTypes array.
 */
void UUIDataStore_OnlinePlaylists::LoadDependentClasses()
{
	Super::LoadDependentClasses();

	// for each configured provider type, load the UIResourceProviderClass associated with that resource type
	if ( ProviderClassName.Len() > 0 )
	{
		ProviderClass = LoadClass<UUIResourceDataProvider>(NULL, *ProviderClassName, NULL, LOAD_None, NULL);
		if ( ProviderClass == NULL )
		{
			debugf(NAME_Warning, TEXT("Failed to load class for %s"), *ProviderClassName);
		}
	}
	else
	{
		debugf(TEXT("No ProviderClassName specified for UUIDataStore_OnlinePlaylists"));
	}
}

/**
 * Called when this data store is added to the data store manager's list of active data stores.
 *
 * Initializes the ListElementProviders map
 *
 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
 *							associated with a particular player; NULL if this is a global data store.
 */
void UUIDataStore_OnlinePlaylists::OnRegister( ULocalPlayer* PlayerOwner )
{
	Super::OnRegister(PlayerOwner);

	InitializeListElementProviders();
}

/**
 * Gets the list of data fields exposed by this data provider.
 *
 * @param	out_Fields	will be filled in with the list of tags which can be used to access data in this data provider.
 *						Will call GetScriptDataTags to allow script-only child classes to add to this list.
 */
void UUIDataStore_OnlinePlaylists::GetSupportedDataFields( TArray<FUIDataProviderField>& out_Fields )
{
	out_Fields.Empty();

	new(out_Fields) FUIDataProviderField( FName(UCONST_RANKEDPROVIDERTAG), *(const TArray<UUIDataProvider*>*) &RankedDataProviders );
	new(out_Fields) FUIDataProviderField( FName(UCONST_UNRANKEDPROVIDERTAG), *(const TArray<UUIDataProvider*>*) &UnrankedDataProviders );
	new(out_Fields) FUIDataProviderField( FName(UCONST_RECMODEPROVIDERTAG), *(const TArray<UUIDataProvider*>*) &RecModeDataProviders );
	new(out_Fields) FUIDataProviderField( FName(UCONST_PRIVATEPROVIDERTAG), *(const TArray<UUIDataProvider*>*) &PrivateDataProviders );

	Super::GetSupportedDataFields(out_Fields);
}

/* === UUIDataStore_OnlinePlaylists interface === */
/**
 * Finds or creates the UIResourceDataProvider instances referenced use by online playlists, and stores the result by ranked or unranked provider types
 */
void UUIDataStore_OnlinePlaylists::InitializeListElementProviders()
{
	RankedDataProviders.Empty();
	UnrankedDataProviders.Empty();
	RecModeDataProviders.Empty();
	PrivateDataProviders.Empty();

	TArray<class UUIResourceDataProvider*>* CurrDataProviderList = NULL;

	// retrieve the list of ini sections that contain data for that provider class	
	TArray<FString> PlaylistSectionNames;
	if ( GConfig->GetPerObjectConfigSections(*ProviderClass->GetConfigName(), *ProviderClass->GetName(), PlaylistSectionNames) )
	{
		for ( INT SectionIndex = 0; SectionIndex < PlaylistSectionNames.Num(); SectionIndex++ )
		{
			INT POCDelimiterPosition = PlaylistSectionNames(SectionIndex).InStr(TEXT(" "));
			// we shouldn't be here if the name was included in the list
			check(POCDelimiterPosition!=INDEX_NONE);

			FName ObjectName = *PlaylistSectionNames(SectionIndex).Left(POCDelimiterPosition);
			if ( ObjectName != NAME_None )
			{
				UOnlinePlaylistProvider* Provider = Cast<UOnlinePlaylistProvider>( StaticFindObject(ProviderClass, ANY_PACKAGE, *ObjectName.ToString(), TRUE) );
				if ( Provider == NULL )
				{
					Provider = ConstructObject<UOnlinePlaylistProvider>(
						ProviderClass,
						this,
						ObjectName
						);
				}

				// Don't add this to the list if it's been disabled from enumeration
				if ( Provider != NULL && !Provider->bSkipDuringEnumeration )
				{
					INT MatchType = eventGetMatchTypeForPlaylistId(Provider->PlaylistId);

					// See the defines in OnlinePlaylistManager
					//Figure out what provider list this provider is going into
					CurrDataProviderList = NULL;
					switch (MatchType)
					{
						// unranked
						case UCONST_PLAYER_MATCH:
						{
							CurrDataProviderList = &UnrankedDataProviders;
							break;
						}
						// rec mode
						case UCONST_REC_MATCH: 
						{
							CurrDataProviderList = &RecModeDataProviders;					
							break;
						}
						// ranked
						case UCONST_RANKED_MATCH:
						{
							CurrDataProviderList = &RankedDataProviders;
							break;
						}
						// private
						case UCONST_PRIVATE_MATCH:
						{
							CurrDataProviderList = &PrivateDataProviders;
							break;
						}
					}
					//Insertion sort the provider into the appropriate list
					if (CurrDataProviderList != NULL)
					{
						INT Idx = 0;
						while(Idx < CurrDataProviderList->Num() && Provider->Priority < Cast<UOnlinePlaylistProvider>((*CurrDataProviderList)(Idx))->Priority )
						{
							Idx++;
						}
						CurrDataProviderList->InsertItem(Provider,Idx);
					}
				}
			}
		}
	}

	for ( INT ProviderIdx = 0; ProviderIdx < RankedDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = RankedDataProviders(ProviderIdx);
		Provider->eventInitializeProvider(!GIsGame);
	}

	for ( INT ProviderIdx = 0; ProviderIdx < UnrankedDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = UnrankedDataProviders(ProviderIdx);
		Provider->eventInitializeProvider(!GIsGame);
	}

	for ( INT ProviderIdx = 0; ProviderIdx < RecModeDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = RecModeDataProviders(ProviderIdx);
		Provider->eventInitializeProvider(!GIsGame);
	}

	for ( INT ProviderIdx = 0; ProviderIdx < PrivateDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = PrivateDataProviders(ProviderIdx);
		Provider->eventInitializeProvider(!GIsGame);
	}
}

/**
 * Get the number of UIResourceDataProvider instances associated with the specified tag.
 *
 * @param	ProviderTag		the tag to find instances for; should match the ProviderTag value of an element in the ElementProviderTypes array.
 *
 * @return	the number of instances registered for ProviderTag.
 */
INT UUIDataStore_OnlinePlaylists::GetProviderCount( FName ProviderTag ) const
{
	if (ProviderTag == UCONST_RANKEDPROVIDERTAG)
	{
		return RankedDataProviders.Num();
	}
	else if ( ProviderTag == UCONST_UNRANKEDPROVIDERTAG)
	{
		return UnrankedDataProviders.Num();
	}
	else if (ProviderTag == UCONST_RECMODEPROVIDERTAG)
	{
		return RecModeDataProviders.Num();
	}
	else if ( ProviderTag == UCONST_PRIVATEPROVIDERTAG)
	{
		return PrivateDataProviders.Num();
	}

	return 0;
}

/**
 * Get the UIResourceDataProvider instances associated with the tag.
 *
 * @param	ProviderTag		the tag to find instances for; should match the ProviderTag value of an element in the ElementProviderTypes array.
 * @param	out_Providers	receives the list of provider instances. this array is always emptied first.
 *
 * @return	the list of UIResourceDataProvider instances registered for ProviderTag.
 */
UBOOL UUIDataStore_OnlinePlaylists::GetResourceProviders( FName ProviderTag, TArray<UUIResourceDataProvider*>& out_Providers ) const
{
	out_Providers.Empty();

	if (ProviderTag == UCONST_RANKEDPROVIDERTAG)
	{
		for ( INT ProviderIndex = 0; ProviderIndex < RankedDataProviders.Num(); ProviderIndex++ )
		{
			 out_Providers.AddItem(RankedDataProviders(ProviderIndex));
		}
	}
	else if ( ProviderTag == UCONST_UNRANKEDPROVIDERTAG)
	{
		for ( INT ProviderIndex = 0; ProviderIndex < UnrankedDataProviders.Num(); ProviderIndex++ )
		{
			out_Providers.AddItem(UnrankedDataProviders(ProviderIndex));
		}
	}
	else if ( ProviderTag == UCONST_RECMODEPROVIDERTAG)
	{
		for ( INT ProviderIndex = 0; ProviderIndex < RecModeDataProviders.Num(); ProviderIndex++ )
		{
			out_Providers.AddItem(RecModeDataProviders(ProviderIndex));
		}
	}
	else if ( ProviderTag == UCONST_PRIVATEPROVIDERTAG)
	{
		for ( INT ProviderIndex = 0; ProviderIndex < PrivateDataProviders.Num(); ProviderIndex++ )
		{
			out_Providers.AddItem(PrivateDataProviders(ProviderIndex));
		}
	}


	return out_Providers.Num() > 0;
}

/**
 * Get the list of fields supported by the provider type specified.
 *
 * @param	ProviderTag			the name of the provider type to get fields for; should match the ProviderTag value of an element in the ElementProviderTypes array.
 *								If the provider type is expanded (bExpandProviders=true), this tag should also contain the array index of the provider instance
 *								to use for retrieving the fields (this can usually be automated by calling GenerateProviderAccessTag)
 * @param	ProviderFieldTags	receives the list of tags supported by the provider specified.
 *
 * @return	TRUE if the tag was resolved successfully and the list of tags was retrieved (doesn't guarantee that the provider
 *			array will contain any elements, though).  FALSE if the data store couldn't resolve the ProviderTag.
 */
UBOOL UUIDataStore_OnlinePlaylists::GetResourceProviderFields( FName ProviderTag, TArray<FName>& ProviderFieldTags ) const
{
	UBOOL bResult = FALSE;

	ProviderFieldTags.Empty();
	TScriptInterface<IUIListElementCellProvider> SchemaProvider = ResolveProviderReference(ProviderTag);
	if ( SchemaProvider )
	{
		// fill the output array with this provider's schema
		TMap<FName,FString> ProviderFieldTagColumnPairs;
		SchemaProvider->GetElementCellTags(ProviderTag, ProviderFieldTagColumnPairs);
		
		ProviderFieldTagColumnPairs.GenerateKeyArray(ProviderFieldTags);
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Get the value of a single field in a specific resource provider instance. Example: GetProviderFieldValue('GameTypes', ClassName, 2, FieldValue)
 *
 * @param	ProviderTag		the name of the provider type; should match the ProviderTag value of an element in the ElementProviderTypes array.
 * @param	SearchField		the name of the field within the provider type to get the value for; should be one of the elements retrieved from a call
 *							to GetResourceProviderFields.
 * @param	ProviderIndex	the index [into the array of providers associated with the specified tag] of the instance to get the value from;
 *							should be one of the elements retrieved by calling GetResourceProviders().
 * @param	out_FieldValue	receives the value of the field
 *
 * @return	TRUE if the field value was successfully retrieved from the provider.  FALSE if the provider tag couldn't be resolved,
 *			the index was not a valid index for the list of providers, or the search field didn't exist in that provider.
 */
UBOOL UUIDataStore_OnlinePlaylists::GetProviderFieldValue( FName ProviderTag, FName SearchField, INT ProviderIndex, FUIProviderScriptFieldValue& out_FieldValue ) const
{
	UBOOL bResult = FALSE;

	UUIResourceDataProvider* FieldValueProvider = NULL;
	if (ProviderTag == UCONST_RANKEDPROVIDERTAG)
	{
		if ( ProviderIndex == INDEX_NONE )
		{
			FieldValueProvider = ResolveProviderReference(ProviderTag, &ProviderIndex);
		}
		else
		{
			if ( RankedDataProviders.IsValidIndex(ProviderIndex) )
			{
				FieldValueProvider = RankedDataProviders(ProviderIndex);
			}
		}
	}
	else if ( ProviderTag == UCONST_UNRANKEDPROVIDERTAG)
	{
		if ( ProviderIndex == INDEX_NONE )
		{
			FieldValueProvider = ResolveProviderReference(ProviderTag, &ProviderIndex);
		}
		else
		{
			if ( UnrankedDataProviders.IsValidIndex(ProviderIndex) )
			{
				FieldValueProvider = UnrankedDataProviders(ProviderIndex);
			}
		}
	}
	else if ( ProviderTag == UCONST_RECMODEPROVIDERTAG)
	{
		if ( ProviderIndex == INDEX_NONE )
		{
			FieldValueProvider = ResolveProviderReference(ProviderTag, &ProviderIndex);
		}
		else
		{
			if ( RecModeDataProviders.IsValidIndex(ProviderIndex) )
			{
				FieldValueProvider = RecModeDataProviders(ProviderIndex);
			}
		}
	}
	else if ( ProviderTag == UCONST_PRIVATEPROVIDERTAG)
	{
		if ( ProviderIndex == INDEX_NONE )
		{
			FieldValueProvider = ResolveProviderReference(ProviderTag, &ProviderIndex);
		}
		else
		{
			if ( PrivateDataProviders.IsValidIndex(ProviderIndex) )
			{
				FieldValueProvider = PrivateDataProviders(ProviderIndex);
			}
		}
	}

	if ( FieldValueProvider != NULL )
	{
		FUIProviderFieldValue FieldValue(EC_EventParm);
		if ( FieldValueProvider->GetCellFieldValue(ProviderTag, SearchField, ProviderIndex, FieldValue, INDEX_NONE) )
		{
			out_FieldValue = FieldValue;
			bResult = TRUE;
		}
	}

	return bResult;
}

/**
 * Searches for resource provider instance that has a field with a value that matches the value specified.
 *
 * @param	ProviderTag			the name of the provider type; should match the ProviderTag value of an element in the ElementProviderTypes array.
 * @param	SearchField			the name of the field within the provider type to compare the value to; should be one of the elements retrieved from a call
 *								to GetResourceProviderFields.
 * @param	ValueToSearchFor	the field value to search for.
 *
 * @return	the index of the resource provider instance that has the same value for SearchField as the value specified, or INDEX_NONE if the
 *			provider tag couldn't be resolved,  none of the provider instances had a field with that name, or none of them had a field
 *			of that name with the value specified.
 */
INT UUIDataStore_OnlinePlaylists::FindProviderIndexByFieldValue( FName ProviderTag, FName SearchField, const FUIProviderScriptFieldValue& ValueToSearchFor ) const
{
	INT Result = INDEX_NONE;

	if (ProviderTag == UCONST_RANKEDPROVIDERTAG)
	{
		FUIProviderFieldValue FieldValue(EC_EventParm);
		for ( INT ProviderIndex = 0; ProviderIndex < RankedDataProviders.Num(); ProviderIndex++ )
		{
			UUIResourceDataProvider* Provider = RankedDataProviders(ProviderIndex);
			if ( Provider->GetCellFieldValue(SearchField, SearchField, ProviderIndex, FieldValue, INDEX_NONE) )
			{
				if ( FieldValue == ValueToSearchFor )
				{
					Result = ProviderIndex;
					break;
				}
			}
		}
	}
	else if ( ProviderTag == UCONST_UNRANKEDPROVIDERTAG)
	{
		FUIProviderFieldValue FieldValue(EC_EventParm);
		for ( INT ProviderIndex = 0; ProviderIndex < UnrankedDataProviders.Num(); ProviderIndex++ )
		{
			UUIResourceDataProvider* Provider = UnrankedDataProviders(ProviderIndex);
			if ( Provider->GetCellFieldValue(SearchField, SearchField, ProviderIndex, FieldValue, INDEX_NONE) )
			{
				if ( FieldValue == ValueToSearchFor )
				{
					Result = ProviderIndex;
					break;
				}
			}
		}
	}
	else if ( ProviderTag == UCONST_RECMODEPROVIDERTAG)
	{
		FUIProviderFieldValue FieldValue(EC_EventParm);
		for ( INT ProviderIndex = 0; ProviderIndex < RecModeDataProviders.Num(); ProviderIndex++ )
		{
			UUIResourceDataProvider* Provider = RecModeDataProviders(ProviderIndex);
			if ( Provider->GetCellFieldValue(SearchField, SearchField, ProviderIndex, FieldValue, INDEX_NONE) )
			{
				if ( FieldValue == ValueToSearchFor )
				{
					Result = ProviderIndex;
					break;
				}
			}
		}
	}
	else if ( ProviderTag == UCONST_PRIVATEPROVIDERTAG)
	{
		FUIProviderFieldValue FieldValue(EC_EventParm);
		for ( INT ProviderIndex = 0; ProviderIndex < PrivateDataProviders.Num(); ProviderIndex++ )
		{
			UUIResourceDataProvider* Provider = PrivateDataProviders(ProviderIndex);
			if ( Provider->GetCellFieldValue(SearchField, SearchField, ProviderIndex, FieldValue, INDEX_NONE) )
			{
				if ( FieldValue == ValueToSearchFor )
				{
					Result = ProviderIndex;
					break;
				}
			}
		}
	}


	return Result;
}

/* === UUIDataStore_OnlinePlaylists interface === */
	
/**
 * Finds the data provider associated with the tag specified.
 *
 * @param	ProviderTag		The tag of the provider to find.  Must match the ProviderTag value for one of elements
 *							in the ElementProviderTypes array, though it can contain an array index (in which case
 *							the array index will be removed from the ProviderTag value passed in).
 * @param	InstanceIndex	If ProviderTag contains an array index, this will be set to the array index value that was parsed.
 *
 * @return	a data provider instance (or CDO if no array index was included in ProviderTag) for the element provider
 *			type associated with ProviderTag.
 */
UUIResourceDataProvider* UUIDataStore_OnlinePlaylists::ResolveProviderReference( FName& ProviderTag, INT* InstanceIndex/*=NULL*/ ) const
{
	UUIResourceDataProvider* Result = NULL;

	if (ProviderTag == UCONST_RANKEDPROVIDERTAG)
	{
		Result = ProviderClass->GetDefaultObject<UUIResourceDataProvider>();
	}
	else if ( ProviderTag == UCONST_UNRANKEDPROVIDERTAG)
	{
		Result = ProviderClass->GetDefaultObject<UUIResourceDataProvider>();
	}
	else if ( ProviderTag == UCONST_PRIVATEPROVIDERTAG)
	{
		Result = ProviderClass->GetDefaultObject<UUIResourceDataProvider>();
	}

	return Result;
}

UBOOL UUIDataStore_OnlinePlaylists::GetPlaylistProvider(FName ProviderTag, INT ProviderIndex, UUIResourceDataProvider*& out_Provider)
{
	out_Provider = NULL;

	if ( IsDataTagSupported(ProviderTag) )
	{
		if (ProviderTag == UCONST_RANKEDPROVIDERTAG)
		{
			if ( RankedDataProviders.IsValidIndex(ProviderIndex) )
			{
				out_Provider = RankedDataProviders(ProviderIndex);
			}
		}
		else if ( ProviderTag == UCONST_UNRANKEDPROVIDERTAG)
		{
			if ( UnrankedDataProviders.IsValidIndex(ProviderIndex) )
			{
				out_Provider = UnrankedDataProviders(ProviderIndex);
			}
		}
		else if ( ProviderTag == UCONST_RECMODEPROVIDERTAG)
		{
			if ( RecModeDataProviders.IsValidIndex(ProviderIndex) )
			{
				out_Provider = RecModeDataProviders(ProviderIndex);
			}
		}
		else if ( ProviderTag == UCONST_PRIVATEPROVIDERTAG)
		{
			if ( PrivateDataProviders.IsValidIndex(ProviderIndex) )
			{
				out_Provider = PrivateDataProviders(ProviderIndex);
			}
		}
	}

	return out_Provider != NULL;
}

/* === IUIListElementProvider interface === */
/**
 * Retrieves the list of all data tags contained by this element provider which correspond to list element data.
 *
 * @return	the list of tags supported by this element provider which correspond to list element data.
 */
TArray<FName> UUIDataStore_OnlinePlaylists::GetElementProviderTags()
{
	TArray<FName> ProviderTags;

	ProviderTags.AddItem(UCONST_RANKEDPROVIDERTAG);
	ProviderTags.AddItem(UCONST_UNRANKEDPROVIDERTAG);
	ProviderTags.AddItem(UCONST_PRIVATEPROVIDERTAG);

	return ProviderTags;
}

/**
 * Returns the number of list elements associated with the data tag specified.
 *
 * @param	FieldName	the name of the property to get the element count for.  guaranteed to be one of the values returned
 *						from GetElementProviderTags.
 *
 * @return	the total number of elements that are required to fully represent the data specified.
 */
INT UUIDataStore_OnlinePlaylists::GetElementCount( FName FieldName )
{
	FString NextFieldName = FieldName.ToString(), FieldTag;
	ParseNextDataTag(NextFieldName, FieldTag);
	if ( IsDataTagSupported(*FieldTag) )
	{
		if (FieldTag == UCONST_RANKEDPROVIDERTAG)
		{
			return RankedDataProviders.Num();
		}
		else if ( FieldTag == UCONST_UNRANKEDPROVIDERTAG)
		{
			return UnrankedDataProviders.Num();
		}
		else if ( FieldTag == UCONST_RECMODEPROVIDERTAG)
		{
			return RecModeDataProviders.Num();
		}
		else if ( FieldTag == UCONST_PRIVATEPROVIDERTAG)
		{
			return PrivateDataProviders.Num();
		}
	}

	return 0;
}

/**
 * Retrieves the list elements associated with the data tag specified.
 *
 * @param	FieldName		the name of the property to get the element count for.  guaranteed to be one of the values returned
 *							from GetElementProviderTags.
 * @param	out_Elements	will be filled with the elements associated with the data specified by DataTag.
 *
 * @return	TRUE if this data store contains a list element data provider matching the tag specified.
 */
UBOOL UUIDataStore_OnlinePlaylists::GetListElements( FName DataTag, TArray<INT>& out_Elements )
{
	UBOOL bResult = FALSE;

	FString NextFieldName = DataTag.ToString(), FieldTag;
	ParseNextDataTag(NextFieldName, FieldTag);
	if ( IsDataTagSupported(*FieldTag) )
	{
		if (FieldTag == UCONST_RANKEDPROVIDERTAG)
		{
			for ( INT ProviderIndex = 0; ProviderIndex < RankedDataProviders.Num(); ProviderIndex++ )
			{
				out_Elements.AddUniqueItem(ProviderIndex);
			}
			bResult = TRUE;
		}
		else if ( FieldTag == UCONST_UNRANKEDPROVIDERTAG)
		{
			for ( INT ProviderIndex = 0; ProviderIndex < UnrankedDataProviders.Num(); ProviderIndex++ )
			{
				out_Elements.AddUniqueItem(ProviderIndex);
			}
			bResult = TRUE;
		}
		else if ( FieldTag == UCONST_RECMODEPROVIDERTAG)
		{
			for ( INT ProviderIndex = 0; ProviderIndex < RecModeDataProviders.Num(); ProviderIndex++ )
			{
				out_Elements.AddUniqueItem(ProviderIndex);
			}
			bResult = TRUE;
		}
		else if ( FieldTag == UCONST_PRIVATEPROVIDERTAG)
		{
			for ( INT ProviderIndex = 0; ProviderIndex < PrivateDataProviders.Num(); ProviderIndex++ )
			{
				out_Elements.AddUniqueItem(ProviderIndex);
			}
			bResult = TRUE;
		}
	}

	return bResult;
}

/**
 * Determines whether a member of a collection should be considered "enabled" by subscribed lists.  Disabled elements will still be displayed in the list
 * but will be drawn using the disabled state.
 *
 * @param	FieldName			the name of the collection data field that CollectionIndex indexes into.
 * @param	CollectionIndex		the index into the data field collection indicated by FieldName to check
 *
 * @return	TRUE if FieldName doesn't correspond to a valid collection data field, CollectionIndex is an invalid index for that collection,
 *			or the item is actually enabled; FALSE only if the item was successfully resolved into a data field value, but should be considered disabled.
 */
UBOOL UUIDataStore_OnlinePlaylists::IsElementEnabled( FName FieldName, INT CollectionIndex )
{
	UBOOL bResult = FALSE;

	FString NextFieldName = FieldName.ToString(), FieldTag;
	ParseNextDataTag(NextFieldName, FieldTag);
	if ( IsDataTagSupported(*FieldTag) )
	{
		if (FieldTag == UCONST_RANKEDPROVIDERTAG)
		{
			if ( RankedDataProviders.IsValidIndex(CollectionIndex) )
			{
				bResult = !RankedDataProviders(CollectionIndex)->eventIsProviderDisabled();
			}
		}
		else if (FieldTag == UCONST_UNRANKEDPROVIDERTAG)
		{
			if ( UnrankedDataProviders.IsValidIndex(CollectionIndex) )
			{
				bResult = !UnrankedDataProviders(CollectionIndex)->eventIsProviderDisabled();
			}
		}
		else if (FieldTag == UCONST_RECMODEPROVIDERTAG)
		{
			if ( RecModeDataProviders.IsValidIndex(CollectionIndex) )
			{
				bResult = !RecModeDataProviders(CollectionIndex)->eventIsProviderDisabled();
			}
		}
		else if ( FieldTag == UCONST_PRIVATEPROVIDERTAG)
		{
			if ( PrivateDataProviders.IsValidIndex(CollectionIndex) )
			{
				bResult = !PrivateDataProviders(CollectionIndex)->eventIsProviderDisabled();
			}
		}
	}

	return bResult;
}

/**
 * Retrieves a list element for the specified data tag that can provide the list with the available cells for this list element.
 * Used by the UI editor to know which cells are available for binding to individual list cells.
 *
 * @param	DataTag			the tag of the list element data provider that we want the schema for.
 *
 * @return	a pointer to some instance of the data provider for the tag specified.  only used for enumerating the available
 *			cell bindings, so doesn't need to actually contain any data (i.e. can be the CDO for the data provider class, for example)
 */
TScriptInterface<IUIListElementCellProvider> UUIDataStore_OnlinePlaylists::GetElementCellSchemaProvider( FName DataTag )
{
	TScriptInterface<IUIListElementCellProvider> Result;

	FString NextFieldName = DataTag.ToString(), FieldTag;
	ParseNextDataTag(NextFieldName, FieldTag);
	if ( IsDataTagSupported(*FieldTag) )
	{
		if (FieldTag == UCONST_RANKEDPROVIDERTAG)
		{
			Result = ProviderClass->GetDefaultObject<UUIResourceDataProvider>();
		}
		else if (FieldTag == UCONST_UNRANKEDPROVIDERTAG)
		{
			Result = ProviderClass->GetDefaultObject<UUIResourceDataProvider>();
		}
		else if ( FieldTag == UCONST_PRIVATEPROVIDERTAG)
		{
			Result = ProviderClass->GetDefaultObject<UUIResourceDataProvider>();
		}
	}

	return Result;
}

/**
 * Retrieves a UIListElementCellProvider for the specified data tag that can provide the list with the values for the cells
 * of the list element indicated by CellValueProvider.DataSourceIndex
 *
 * @param	FieldName		the tag of the list element data field that we want the values for
 * @param	ListIndex		the list index for the element to get values for
 * 
 * @return	a pointer to an instance of the data provider that contains the value for the data field and list index specified
 */
TScriptInterface<IUIListElementCellProvider> UUIDataStore_OnlinePlaylists::GetElementCellValueProvider( FName FieldName, INT ListIndex )
{
	TScriptInterface<IUIListElementCellProvider> Result;

	FString NextFieldName = FieldName.ToString(), FieldTag;
	ParseNextDataTag(NextFieldName, FieldTag);
	if ( IsDataTagSupported(*FieldTag) )
	{
		if (FieldTag == UCONST_RANKEDPROVIDERTAG)
		{
			if ( RankedDataProviders.IsValidIndex(ListIndex) )
			{
				Result = RankedDataProviders(ListIndex);
			}
		}
		else if (FieldTag == UCONST_UNRANKEDPROVIDERTAG)
		{
			if ( UnrankedDataProviders.IsValidIndex(ListIndex) )
			{
				Result = UnrankedDataProviders(ListIndex);
			}
		}
		else if (FieldTag == UCONST_RECMODEPROVIDERTAG)
		{
			if ( RecModeDataProviders.IsValidIndex(ListIndex) )
			{
				Result = RecModeDataProviders(ListIndex);
			}
		}
		else if ( FieldTag == UCONST_PRIVATEPROVIDERTAG)
		{
			if ( PrivateDataProviders.IsValidIndex(ListIndex) )
			{
				Result = PrivateDataProviders(ListIndex);
			}
		}
	}

	return Result;
}

/**
 * Resolves PropertyName into a list element provider that provides list elements for the property specified.
 *
 * @param	PropertyName	the name of the property that corresponds to a list element provider supported by this data store
 *
 * @return	a pointer to an interface for retrieving list elements associated with the data specified, or NULL if
 *			there is no list element provider associated with the specified property.
 */
TScriptInterface<IUIListElementProvider> UUIDataStore_OnlinePlaylists::ResolveListElementProvider( const FString& PropertyName )
{
	TScriptInterface<IUIListElementProvider> Result;
	TArray<FUIDataProviderField> SupportedFields;

	FString NextFieldName = PropertyName, FieldTag;
	ParseNextDataTag(NextFieldName, FieldTag);
	while ( FieldTag.Len() > 0 ) 
	{
		if ( IsDataTagSupported(*FieldTag, SupportedFields) )
		{
			Result = this;
		}

		ParseNextDataTag(NextFieldName, FieldTag);
	}

	if ( !Result )
	{
		Result = Super::ResolveListElementProvider(PropertyName);
	}

	return Result;
}

/* === UIDataProvider interface === */
/**
 * Resolves the value of the data field specified and stores it in the output parameter.
 *
 * @param	FieldName		the data field to resolve the value for;  guaranteed to correspond to a property that this provider
 *							can resolve the value for (i.e. not a tag corresponding to an internal provider, etc.)
 * @param	out_FieldValue	receives the resolved value for the property specified.
 *							@see GetDataStoreValue for additional notes
 * @param	ArrayIndex		optional array index for use with data collections
 */
UBOOL UUIDataStore_OnlinePlaylists::GetFieldValue( const FString& FieldName, FUIProviderFieldValue& out_FieldValue, INT ArrayIndex/*=INDEX_NONE*/ )
{
	UBOOL bResult = FALSE;

	FName FieldTag(*FieldName);
	TArray<INT> ProviderIndexes;
	if ( GetListElements(FieldTag, ProviderIndexes) )
	{
		for (int ProviderIdx = 0; ProviderIdx < ProviderIndexes.Num() ; ProviderIdx++)
		{
			if ( IsElementEnabled(FieldTag, ProviderIdx) )
			{
				out_FieldValue.PropertyTag = FieldTag;
				out_FieldValue.PropertyType = DATATYPE_ProviderCollection;
				out_FieldValue.ArrayValue.AddItem(ProviderIndexes(ProviderIdx));
				bResult = TRUE;
				break;
			}
		}
	}

	return bResult || Super::GetFieldValue(FieldName,out_FieldValue,ArrayIndex);
}

/**
 * Generates filler data for a given tag.  This is used by the editor to generate a preview that gives the
 * user an idea as to what a bound datastore will look like in-game.
 *
 * @param		DataTag		the tag corresponding to the data field that we want filler data for
 *
 * @return		a string of made-up data which is indicative of the typical [resolved] value for the specified field.
 */
FString UUIDataStore_OnlinePlaylists::GenerateFillerData( const FString& DataTag )
{
	//@todo
	return Super::GenerateFillerData(DataTag);
}

/* === UObject interface === */
/** Required since maps are not yet supported by script serialization */
void UUIDataStore_OnlinePlaylists::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects(ObjectArray);

	for ( INT ProviderIndex = 0; ProviderIndex < RankedDataProviders.Num(); ProviderIndex++ )
	{
		UUIResourceDataProvider* ResourceProvider = RankedDataProviders(ProviderIndex);
		if ( ResourceProvider != NULL )
		{
			AddReferencedObject(ObjectArray, ResourceProvider);
		}
	}

	for ( INT ProviderIndex = 0; ProviderIndex < UnrankedDataProviders.Num(); ProviderIndex++ )
	{
		UUIResourceDataProvider* ResourceProvider = UnrankedDataProviders(ProviderIndex);
		if ( ResourceProvider != NULL )
		{
			AddReferencedObject(ObjectArray, ResourceProvider);
		}
	}
}

void UUIDataStore_OnlinePlaylists::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	if ( !Ar.IsPersistent() )
	{
		for ( INT ProviderIndex = 0; ProviderIndex < RankedDataProviders.Num(); ProviderIndex++ )
		{
			UUIResourceDataProvider* ResourceProvider = RankedDataProviders(ProviderIndex);
			Ar << ResourceProvider;
		}

		for ( INT ProviderIndex = 0; ProviderIndex < UnrankedDataProviders.Num(); ProviderIndex++ )
		{
			UUIResourceDataProvider* ResourceProvider = UnrankedDataProviders(ProviderIndex);
			Ar << ResourceProvider;
		}
	}
}

/**
 * Called from ReloadConfig after the object has reloaded its configuration data.
 */
void UUIDataStore_OnlinePlaylists::PostReloadConfig( UProperty* PropertyThatWasLoaded )
{
	Super::PostReloadConfig( PropertyThatWasLoaded );

	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		if ( PropertyThatWasLoaded == NULL || PropertyThatWasLoaded->GetFName() == TEXT("ProviderClassName") )
		{
			// reload the ProviderClass
			LoadDependentClasses();

			// the current list element providers are potentially no longer accurate, so we'll need to reload that list as well.
			InitializeListElementProviders();

			// now notify any widgets that are subscribed to this datastore that they need to re-resolve their bindings
			eventRefreshSubscribers();
		}
	}
}

/**
 * Callback for retrieving a textual representation of natively serialized properties.  Child classes should implement this method if they wish
 * to have natively serialized property values included in things like diffcommandlet output.
 *
 * @param	out_PropertyValues	receives the property names and values which should be reported for this object.  The map's key should be the name of
 *								the property and the map's value should be the textual representation of the property's value.  The property value should
 *								be formatted the same way that UProperty::ExportText formats property values (i.e. for arrays, wrap in quotes and use a comma
 *								as the delimiter between elements, etc.)
 * @param	ExportFlags			bitmask of EPropertyPortFlags used for modifying the format of the property values
 *
 * @return	return TRUE if property values were added to the map.
 */
UBOOL UUIDataStore_OnlinePlaylists::GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, DWORD ExportFlags/*=0*/ ) const
{
	UBOOL bResult = Super::GetNativePropertyValues(out_PropertyValues, ExportFlags);

	INT Count=0, LongestProviderTag=0;
	TMap<FString,FString> PropertyValues;
	for ( INT ProviderIdx = 0; ProviderIdx < RankedDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = RankedDataProviders(ProviderIdx);
		FString PropertyName = *FString::Printf(TEXT("RankedPlaylistProviders[%i]"), ProviderIdx);
		FString PropertyValue = Provider->GetName();

		LongestProviderTag = Max(LongestProviderTag, PropertyName.Len());
		PropertyValues.Set(*PropertyName, PropertyValue);
	}

	for ( INT ProviderIdx = 0; ProviderIdx < UnrankedDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = UnrankedDataProviders(ProviderIdx);
		FString PropertyName = *FString::Printf(TEXT("UnrankedPlaylistProviders[%i]"), ProviderIdx);
		FString PropertyValue = Provider->GetName();

		LongestProviderTag = Max(LongestProviderTag, PropertyName.Len());
		PropertyValues.Set(*PropertyName, PropertyValue);
	}

	for ( TMap<FString,FString>::TConstIterator It(PropertyValues); It; ++It )
	{
		const FString& ProviderTag = It.Key();
		const FString& ProviderName = It.Value();
	
		out_PropertyValues.Set(*ProviderTag, ProviderName.LeftPad(LongestProviderTag));
		bResult = TRUE;
	}

	return bResult;
}

#endif
