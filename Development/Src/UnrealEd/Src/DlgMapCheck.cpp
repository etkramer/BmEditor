/*=============================================================================
	DlgMapCheck.cpp: UnrealEd dialog for displaying map errors.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgMapCheck.h"
#include "EngineSequenceClasses.h"
#include "BusyCursor.h"
#include "ScopedTransaction.h"

BEGIN_EVENT_TABLE(WxDlgMapCheck, WxTrackableDialog)
	EVT_BUTTON(wxID_CANCEL,						WxDlgMapCheck::OnClose)
	EVT_BUTTON(IDPB_REFRESH,					WxDlgMapCheck::OnRefresh)
	EVT_BUTTON(IDPB_GOTOACTOR,					WxDlgMapCheck::OnGoTo)
	EVT_BUTTON(IDPB_DELETEACTOR,				WxDlgMapCheck::OnDelete)
	EVT_BUTTON(IDPB_GOTOPROPERTIES,				WxDlgMapCheck::OnGotoProperties)
	EVT_BUTTON(IDPB_SHOWHELPPAGE,				WxDlgMapCheck::OnShowHelpPage)
	EVT_BUTTON(IDPB_COPYTOCLIPBOARD,			WxDlgMapCheck::OnCopyToClipboard)
	EVT_LIST_ITEM_ACTIVATED(IDLC_ERRORWARNING,	WxDlgMapCheck::OnItemActivated)

	EVT_LIST_COL_CLICK(IDLC_ERRORWARNING,		WxDlgMapCheck::OnColumnClicked)

	EVT_UPDATE_UI(IDPB_GOTOACTOR,				WxDlgMapCheck::OnUpdateUI)
	EVT_UPDATE_UI(IDPB_DELETEACTOR,				WxDlgMapCheck::OnUpdateUI)
	EVT_UPDATE_UI(IDPB_GOTOPROPERTIES,			WxDlgMapCheck::OnUpdateUI)
	EVT_UPDATE_UI(IDPB_SHOWHELPPAGE,			WxDlgMapCheck::OnUpdateUI)

	EVT_CHECKBOX(IDCB_CRITICALERRORFILTER,		WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_ERRORFILTER,				WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_PERFWARNINGFILTER,		WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_WARNINGFILTER,			WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_NOTEFILTER,				WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_KISMETFILTER,				WxDlgMapCheck::OnFilter)
	EVT_CHECKBOX(IDCB_INFOFILTER,				WxDlgMapCheck::OnFilter)

	EVT_UPDATE_UI(IDCB_CRITICALERRORFILTER,		WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_ERRORFILTER,				WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_PERFWARNINGFILTER,		WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_WARNINGFILTER,			WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_NOTEFILTER,				WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_KISMETFILTER,			WxDlgMapCheck::OnUpdateFilterUI)
	EVT_UPDATE_UI(IDCB_INFOFILTER,				WxDlgMapCheck::OnUpdateFilterUI)
END_EVENT_TABLE()

WxDlgMapCheck::WxDlgMapCheck(wxWindow* InParent) : 
	WxTrackableDialog(InParent, wxID_ANY, (wxString)*LocalizeUnrealEd("MapCheck"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	DlgPosSizeName(TEXT("DlgMapCheckWithFilters"))
{
	ErrorTypes[MCTYPE_CRITICALERROR] = LocalizeUnrealEd("CriticalErrorFilter");
	ErrorTypes[MCTYPE_ERROR] = LocalizeUnrealEd("ErrorFilter");
	ErrorTypes[MCTYPE_PERFORMANCEWARNING] = LocalizeUnrealEd("PerformanceWarningFilter");
	ErrorTypes[MCTYPE_WARNING] = LocalizeUnrealEd("WarningFilter");
	ErrorTypes[MCTYPE_NOTE] = LocalizeUnrealEd("NoteFilter");
	ErrorTypes[MCTYPE_KISMET] = LocalizeUnrealEd("KismetFilter");
	ErrorTypes[MCTYPE_INFO] = LocalizeUnrealEd("InfoFilter");

	ErrorTypeColumnLabels[MCTYPE_CRITICALERROR] = LocalizeUnrealEd("CriticalErrorColumn");
	ErrorTypeColumnLabels[MCTYPE_ERROR] = LocalizeUnrealEd("ErrorColumn");
	ErrorTypeColumnLabels[MCTYPE_PERFORMANCEWARNING] = LocalizeUnrealEd("PerformanceWarningColumn");
	ErrorTypeColumnLabels[MCTYPE_WARNING] = LocalizeUnrealEd("WarningColumn");
	ErrorTypeColumnLabels[MCTYPE_NOTE] = LocalizeUnrealEd("NoteColumn");
	ErrorTypeColumnLabels[MCTYPE_KISMET] = LocalizeUnrealEd("KismetColumn");
	ErrorTypeColumnLabels[MCTYPE_INFO] = LocalizeUnrealEd("InfoColumn");
}

WxDlgMapCheck::~WxDlgMapCheck()
{
	// Save window position.
	FWindowUtil::SavePosSize(*DlgPosSizeName, this);
}

/**
 *	Initialize the dialog box.
 */
void WxDlgMapCheck::Initialize()
{
	wxBoxSizer* MainSizer = new wxBoxSizer(wxHORIZONTAL);
	{
		// Warning List
		wxBoxSizer* ListSizer = new wxBoxSizer(wxVERTICAL);
		{
			ErrorWarningList = new wxListCtrl( this, IDLC_ERRORWARNING, wxDefaultPosition, wxSize(350, 200), wxLC_REPORT );
			ListSizer->Add(ErrorWarningList, 1, wxGROW|wxALL, 5);
		}
		MainSizer->Add(ListSizer, 1, wxGROW|wxALL, 5);		

		// Add buttons
		wxBoxSizer* ButtonSizer = new wxBoxSizer(wxVERTICAL);
		{
			{
				CriticalErrorCheckBox = new wxCheckBox(this, IDCB_CRITICALERRORFILTER, *ErrorTypes[MCTYPE_CRITICALERROR]);
				ButtonSizer->Add(CriticalErrorCheckBox, 0, wxEXPAND|wxALL, 5);

				ErrorCheckBox = new wxCheckBox(this, IDCB_ERRORFILTER, *ErrorTypes[MCTYPE_ERROR]);
				ButtonSizer->Add(ErrorCheckBox, 0, wxEXPAND|wxALL, 5);

				PerfWarningCheckBox = new wxCheckBox(this, IDCB_PERFWARNINGFILTER, *ErrorTypes[MCTYPE_PERFORMANCEWARNING]);
				ButtonSizer->Add(PerfWarningCheckBox, 0, wxEXPAND|wxALL, 5);

				WarningCheckBox = new wxCheckBox(this, IDCB_WARNINGFILTER, *ErrorTypes[MCTYPE_WARNING]);
				ButtonSizer->Add(WarningCheckBox, 0, wxEXPAND|wxALL, 5);

				NoteCheckBox = new wxCheckBox(this, IDCB_NOTEFILTER, *ErrorTypes[MCTYPE_NOTE]);
				ButtonSizer->Add(NoteCheckBox, 0, wxEXPAND|wxALL, 5);

				KismetCheckBox = new wxCheckBox(this, IDCB_KISMETFILTER, *ErrorTypes[MCTYPE_KISMET]);
				ButtonSizer->Add(KismetCheckBox, 0, wxEXPAND|wxALL, 5);

				InfoCheckBox = new wxCheckBox(this, IDCB_INFOFILTER, *ErrorTypes[MCTYPE_INFO]);
				ButtonSizer->Add(InfoCheckBox, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			{
				CloseButton = new wxButton( this, wxID_OK, *LocalizeUnrealEd("Close"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(CloseButton, 0, wxEXPAND|wxALL, 5);

				RefreshButton = new wxButton( this, IDPB_REFRESH, *LocalizeUnrealEd("RefreshF5"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(RefreshButton, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			{
				GotoButton = new wxButton( this, IDPB_GOTOACTOR, *LocalizeUnrealEd("GoTo"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(GotoButton, 0, wxEXPAND|wxALL, 5);

				PropertiesButton = new wxButton( this, IDPB_GOTOPROPERTIES, *LocalizeUnrealEd("Properties"), wxDefaultPosition, wxDefaultSize, 0 );
				ButtonSizer->Add(PropertiesButton, 0, wxEXPAND|wxALL, 5);
			}

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			DeleteButton = new wxButton( this, IDPB_DELETEACTOR, *LocalizeUnrealEd("Delete"), wxDefaultPosition, wxDefaultSize, 0 );
			ButtonSizer->Add(DeleteButton, 0, wxEXPAND|wxALL, 5);

			ButtonSizer->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

			{
				HelpButton = new wxButton(this, IDPB_SHOWHELPPAGE, *LocalizeUnrealEd("UDNWarningErrorHelpF1"));
				ButtonSizer->Add(HelpButton, 0, wxEXPAND|wxALL, 5);
			
				CopyToClipboardButton = new wxButton(this, IDPB_COPYTOCLIPBOARD, *LocalizeUnrealEd("CopyToClipboard") );
				ButtonSizer->Add(CopyToClipboardButton, 0, wxEXPAND|wxALL, 5 );
			}
		}
		MainSizer->Add(ButtonSizer, 0, wxALIGN_TOP|wxALL, 5);

	}
	SetSizer(MainSizer);

	// Set an accelerator table to handle hotkey presses for the window.
	wxAcceleratorEntry Entries[3];
	Entries[0].Set(wxACCEL_NORMAL,  WXK_F5,			IDPB_REFRESH);
	Entries[1].Set(wxACCEL_NORMAL,  WXK_DELETE,		IDPB_DELETEACTOR);
	Entries[2].Set(wxACCEL_NORMAL,  WXK_F1,			IDPB_SHOWHELPPAGE);

	wxAcceleratorTable AcceleratorTable(3, Entries);
	SetAcceleratorTable(AcceleratorTable);

	// Fill in the columns
	SetupErrorWarningListColumns();

	// Entries in the imagelist must match the ordering of the MCTYPE_* enum in Core.h
	ImageList = new wxImageList( 16, 15 );
	ImageList->Add( WxBitmap( "CriticalError" ), wxColor( 192,192,192 ) );
	ImageList->Add( WxBitmap( "Error" ), wxColor( 192,192,192 ) );
	ImageList->Add( WxBitmap( "PerformanceWarning" ), wxColor( 255,255,192 ) );
	ImageList->Add( WxBitmap( "Warning" ), wxColor( 192,192,192 ) );
	ImageList->Add( WxBitmap( "Note" ), wxColor( 192,192,192 ) );
	ImageList->Add( WxBitmap( "Kismet" ), wxColor( 192,192,192 ) );
	ImageList->Add( WxBitmap( "Info" ), wxColor( 192,192,192 ) ); 
	ErrorWarningList->AssignImageList( ImageList, wxIMAGE_LIST_SMALL );

	// init filter settings
	ListFilter[MCTYPE_CRITICALERROR] = TRUE;
	ListFilter[MCTYPE_ERROR] = TRUE;
	ListFilter[MCTYPE_PERFORMANCEWARNING] = TRUE;
	ListFilter[MCTYPE_WARNING] = TRUE;
	ListFilter[MCTYPE_NOTE] = TRUE;
	ListFilter[MCTYPE_KISMET] = TRUE;
	ListFilter[MCTYPE_INFO] = TRUE;
	OnSetFilterCheckBoxes();

	// Load window position.
	FWindowUtil::LoadPosSize(*DlgPosSizeName, this, 490, 275, 900, 540);

	ErrorWarningInfoList = &GErrorWarningInfoList;

	SortColumn = -1;
}

/**
 * This function is called when the window has been selected from within the ctrl + tab dialog.
 */
void WxDlgMapCheck::OnSelected()
{
	if(!IsShown())
	{
		Show();
	}

	WxTrackableDialog::OnSelected();
}

bool WxDlgMapCheck::Show( bool show )
{
	if( show )
	{
		LoadListCtrl();
	}
	const bool bShown = wxDialog::Show(show);
	
	// Force the dialog to the front
	Raise();
	return bShown;
}

/**
 * Shows the dialog only if there are messages to display.
 */
void WxDlgMapCheck::ShowConditionally()
{
	LoadListCtrl();

	if( ErrorWarningList->GetItemCount() > 0 )
	{
		Show( true );
	}
}

/** Clears out the list of messages appearing in the window. */
void WxDlgMapCheck::ClearMessageList()
{
	ErrorWarningList->DeleteAllItems();
	ErrorWarningInfoList->Empty();
	ReferencedObjects.Empty();
}

/**
 * Freezes the message list.
 */
void WxDlgMapCheck::FreezeMessageList()
{
	ErrorWarningList->Freeze();
}

/**
 * Thaws the message list.
 */
void WxDlgMapCheck::ThawMessageList()
{
	ErrorWarningList->Thaw();
}

/**
 * Returns TRUE if the dialog has any map errors, FALSE if not
 *
 * @return TRUE if the dialog has any map errors, FALSE if not
 */
UBOOL WxDlgMapCheck::HasAnyErrors() const
{
	UBOOL bHasErrors = FALSE;
	if ( ErrorWarningInfoList )
	{
		for ( TArray<FCheckErrorWarningInfo>::TConstIterator InfoIter( *ErrorWarningInfoList ); InfoIter; ++InfoIter )
		{
			const FCheckErrorWarningInfo& CurInfo = *InfoIter;
			if ( CurInfo.Type == MCTYPE_ERROR || CurInfo.Type == MCTYPE_CRITICALERROR )
			{
				bHasErrors = TRUE;
				break;
			}
		}
	}
	return bHasErrors;
}

/**
 * Adds a message to the map check dialog, to be displayed when the dialog is shown.
 *
 * @param	InType					The type of message.
 * @param	InActor					Actor associated with the message; can be NULL.
 * @param	InMessage				The message to display.
 * @param	InRecommendedAction		The recommended course of action to take.
 * @param	InUDNPage				UDN Page to visit if the user needs more info on the warning.  This will send the user to https://udn.epicgames.com/Three/MapErrors#InUDNPage. 
 */
void WxDlgMapCheck::AddItem(MapCheckType InType, UObject* InActor, const TCHAR* InMessage, MapCheckAction InRecommendedAction, const TCHAR* InUDNPage)
{
	// Columns are, from left to right: Type, Level, Actor, Message.

	FCheckErrorWarningInfo ErrorWarningInfo;

	ErrorWarningInfo.RecommendedAction = InRecommendedAction;
	ErrorWarningInfo.UDNHelpString = InUDNPage;
	ErrorWarningInfo.Object = InActor;
	ErrorWarningInfo.Type = InType;
	ErrorWarningInfo.Message = InMessage;
	ErrorWarningInfo.UDNPage = InUDNPage;

	GErrorWarningInfoList.AddItem( ErrorWarningInfo );
}

/**
* Loads the list control with the contents of the GErrorWarningInfoList array.
*/
void WxDlgMapCheck::LoadListCtrl()
{
	FString ActorName( TEXT("<None>") );
	FString LevelName( TEXT("<None>") );
	UBOOL bReferencedByKismet = FALSE;

	ErrorWarningList->DeleteAllItems();
	ReferencedObjects.Empty();

	SortErrorWarningInfoList();

	for( int x = 0 ; x < ErrorWarningInfoList->Num() ; ++x )
	{
		FCheckErrorWarningInfo* ewi = &((*ErrorWarningInfoList)( x ));

		if ( ewi->Object )
		{
			LevelName = GetLevelOrPackageName(ewi->Object);

			ActorName = ewi->Object->GetName();
			ULevel* Level = NULL;
			AActor* Actor = Cast<AActor>(ewi->Object);
			if (Actor)
			{
				Level = Actor->GetLevel();
			}

			if ((ewi->Type != MCTYPE_KISMET && ewi->Type != MCTYPE_PERFORMANCEWARNING) && Level)
			{
				// Determine if the actor is referenced by a kismet sequence.
				USequence* RootSeq = GWorld->GetGameSequence( Level );
				if( RootSeq )
				{
					bReferencedByKismet = RootSeq->ReferencesObject(Actor);
				}
			}
		}
		else
		{
			ActorName = TEXT("<none>");
			LevelName = TEXT("");
		}

		if (ListFilter[ewi->Type])
		{
			const LONG Index = ErrorWarningList->InsertItem( x, *ErrorTypeColumnLabels[ewi->Type], bReferencedByKismet ? MCTYPE_KISMET : ewi->Type );
			ErrorWarningList->SetItem( Index, 1, *LevelName );
			ErrorWarningList->SetItem( Index, 2, *ActorName );
			ErrorWarningList->SetItem( Index, 3, *ewi->Message );
			ErrorWarningList->SetItemPtrData( Index, (PTRINT)ewi );
		}

		if (ewi->Object)
		{
			ReferencedObjects.AddUniqueItem(ewi->Object);
		}
	}
}

void WxDlgMapCheck::Serialize(FArchive& Ar)
{
	Ar << ReferencedObjects;
}

/**
 * Removes all items from the map check dialog that pertain to the specified object.
 *
 * @param	Object		The object to match when removing items.
 */
void WxDlgMapCheck::RemoveObjectItems(UObject* Object)
{
	// Remove object from the referenced object's array.
	ReferencedObjects.RemoveItem(Object);

	// Loop through all of the items in our warning list and remove them from the list view if their client data matches the actor we are removing,
	// make sure to iterate through the list backwards so we do not modify ids as we are removing items.
	INT InfoListCount = ErrorWarningInfoList->Num();
	for(long InfoItemIdx=InfoListCount-1; InfoItemIdx>=0;InfoItemIdx--)
	{
		const UObject* ItemObject = (*ErrorWarningInfoList)(InfoItemIdx).Object;
		if ( ItemObject == Object )
		{
			ErrorWarningInfoList->Remove((INT)InfoItemIdx);
		}
	}

	INT ListCount = ErrorWarningList->GetItemCount();
	for(long ItemIdx=ListCount-1; ItemIdx>=0;ItemIdx--)
	{
		const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIdx);
		if ( ItemInfo->Object == Object )
		{
			ErrorWarningList->DeleteItem(ItemIdx);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Event handlers.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Event handler for when the close button is clicked on. */
void WxDlgMapCheck::OnClose(wxCommandEvent& In)
{
	Show(0);
}

/** Event handler for when the refresh button is clicked on. */
void WxDlgMapCheck::OnRefresh(wxCommandEvent& In)
{
	GEditor->Exec( TEXT("MAP CHECK") );
	LoadListCtrl();
}

/** Event handler for when the goto button is clicked on. */
void WxDlgMapCheck::OnGoTo(wxCommandEvent& In)
{
	const INT NumSelected = ErrorWarningList->GetSelectedItemCount();

	if( NumSelected > 0 )
	{
		const FScopedBusyCursor BusyCursor;
		TArray<AActor*> SelectedActors;
		TArray<UPrimitiveComponent*> SelectedComponents;
		TArray<UObject*> SelectedObjects;
		long ItemIndex = ErrorWarningList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		while( ItemIndex != -1 )
		{
			const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
			UObject* Object = ItemInfo->Object;
			if ( Object )
			{
				AActor* Actor = Cast<AActor>(Object);
				UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Object);

				if ( Actor )
				{
					SelectedActors.AddItem( Actor );
				}
				else
				if (Component)
				{
					SelectedComponents.AddItem(Component);
				}
				else
				{
					SelectedObjects.AddItem(Object);
				}
			}
			ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		}

		// If any components are selected, find the actor(s) they are associated with
		if (SelectedComponents.Num() > 0)
		{
			for (FActorIterator ActorIt; ActorIt; ++ActorIt)
			{
				for (INT CompIdx = 0; CompIdx < ActorIt->AllComponents.Num(); CompIdx++)
				{
					UPrimitiveComponent* CheckPrim = Cast<UPrimitiveComponent>(ActorIt->AllComponents(CompIdx));
					if (CheckPrim)
					{
						INT TempIdx;
						if (SelectedComponents.FindItem(CheckPrim, TempIdx))
						{
							SelectedActors.AddUniqueItem(*ActorIt);
							break;
						}
					}
				}
			}
		}

		// Selecting actors gets priority
		if (SelectedActors.Num() > 0)
		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("MapCheckGoto") );
			GEditor->SelectNone( FALSE, TRUE );
			for ( INT ActorIndex = 0 ; ActorIndex < SelectedActors.Num() ; ++ActorIndex )
			{
				AActor* Actor = SelectedActors(ActorIndex);
				GEditor->SelectActor( Actor, TRUE, NULL, FALSE, TRUE );
			}
			GEditor->NoteSelectionChange();
			GEditor->MoveViewportCamerasToActor( SelectedActors, FALSE );
		}
		else
		if (SelectedObjects.Num() > 0)
		{
			GApp->EditorFrame->SyncBrowserToObjects(SelectedObjects);
		}
	}
}

/** Event handler for when the delete button is clicked on. */
void WxDlgMapCheck::OnDelete(wxCommandEvent& In)
{
	const FScopedBusyCursor BusyCursor;
	GEditor->Exec(TEXT("ACTOR SELECT NONE"));

	long ItemIndex = ErrorWarningList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	TArray<AActor*> RemoveList;

	while( ItemIndex != -1 )
	{
		const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
		UObject* Object = ItemInfo->Object;
		if (Object)
		{
			AActor* Actor = Cast<AActor>(Object);
			if ( Actor )
			{
				GEditor->SelectActor( Actor, TRUE, NULL, FALSE, TRUE );

				if(Actor != NULL)
				{
					RemoveList.AddUniqueItem(Actor);
				}
			}
		}
		ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	}

	GEditor->Exec( TEXT("DELETE") );
	
	// Loop through all of the actors we deleted and remove any items that reference it.
	for(INT RemoveIdx=0; RemoveIdx < RemoveList.Num(); RemoveIdx++)
	{
		// only remove the item if the delete was successful
		AActor* Actor = RemoveList(RemoveIdx);
		if ( Actor->IsPendingKill() )
		{
			RemoveObjectItems( Actor );
		}
	}
}

/** Event handler for when a the properties button is clicked on */
void WxDlgMapCheck::OnGotoProperties(wxCommandEvent& In)
{
	const INT NumSelected = ErrorWarningList->GetSelectedItemCount();

	if( NumSelected > 0 )
	{
		const FScopedBusyCursor BusyCursor;
		TArray< UObject* > SelectedActors;
		TArray<UPrimitiveComponent*> SelectedComponents;

		// Find all actors or components selected in the error list.
		LONG ItemIndex = ErrorWarningList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		while( ItemIndex != -1 )
		{
			const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
			UObject* Object = ItemInfo->Object;
			if (Object)
			{
				AActor* Actor = Cast<AActor>(Object);
				UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Object);

				if ( Actor )
				{
					SelectedActors.AddItem( Actor );
				}
				else if ( Component )
				{
					SelectedComponents.AddItem( Component );
				}
		
			}
			ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		}

		// If any components are selected, find the actor(s) they are associated with
		for (INT ComponentScan = 0; ComponentScan < SelectedComponents.Num(); ++ComponentScan)
		{
			AActor* ParentActor = SelectedComponents(ComponentScan)->GetOwner();
			if (ParentActor)
			{
				SelectedActors.AddUniqueItem( ParentActor );
			}
		}

		if ( SelectedActors.Num() > 0 )
		{
			// Update the property windows and create one if necessary
			GUnrealEd->ShowActorProperties();
			// Only show properties for the actors that were selected in the error list.
			GUnrealEd->UpdatePropertyWindowFromActorList( SelectedActors );
		}
	}
}

/** Event handler for when a message is clicked on. */
void WxDlgMapCheck::OnItemActivated(wxListEvent& In)
{
	const long ItemIndex = In.GetIndex();
	const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
	UObject* Obj = ItemInfo->Object;
	AActor* Actor = Cast<AActor>( Obj );
	UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Obj);
	if (Component)
	{
		UModelComponent* ModelComponent = Cast<UModelComponent>(Component);
		if (ModelComponent)
		{
			ModelComponent->SelectAllSurfaces();
		}
		else
		{
			for (FActorIterator ActorIt; ActorIt; ++ActorIt)
			{
				for (INT CompIdx = 0; CompIdx < ActorIt->AllComponents.Num(); CompIdx++)
				{
					UPrimitiveComponent* CheckPrim = Cast<UPrimitiveComponent>(ActorIt->AllComponents(CompIdx));
					if (CheckPrim)
					{
						if (CheckPrim == Component)
						{
							Actor = *ActorIt;
							break;
						}
					}
				}
			}
		}
	}

	if ( Actor )
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd("MapCheckGoto") );
		GEditor->SelectNone( TRUE, TRUE );
		GEditor->SelectActor( Actor, TRUE, NULL, TRUE, TRUE );
		GEditor->MoveViewportCamerasToActor( *Actor, FALSE );
	}
	else
	if (Obj)
	{
		TArray<UObject*> SelectedObjects;
		SelectedObjects.AddItem(Obj);
		GApp->EditorFrame->SyncBrowserToObjects(SelectedObjects);
	}
}

/** Event handler for when a column is clicked on. */
void WxDlgMapCheck::OnColumnClicked (wxListEvent& In)
{
	SortColumn = In.GetColumn();
	LoadListCtrl();
}

/** Event handler for when the "Show Help" button is clicked on. */
void WxDlgMapCheck::OnShowHelpPage(wxCommandEvent& In)
{
#if UDK
	// UDK Users get sent to the UDK Homepage
	wxLaunchDefaultBrowser(TEXT("http://udn.epicgames.com/Three/DevelopmentKitHome.html"));
#else
#if SHIPPING_PC_GAME
	// End users don't have access to the secure parts of UDN; send them to the UDN homepage.
	wxLaunchDefaultBrowser(TEXT("http://udn.epicgames.com/Main/WebHome.html"));
#else
	// Send developers, who have access to the secure parts of UDN, to the context-specific page.
	const FString BaseURLString = "https://udn.epicgames.com/Three/MapErrors";

	// Loop through all selected items and launch browser pages for each item.
	long ItemIndex = ErrorWarningList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	while( ItemIndex != -1 )
	{
		const FCheckErrorWarningInfo* ItemInfo = (FCheckErrorWarningInfo*)ErrorWarningList->GetItemData(ItemIndex);
		
		if(ItemInfo->UDNHelpString.Len())
		{
			wxLaunchDefaultBrowser(*FString::Printf(TEXT("%s#%s"), *BaseURLString, *ItemInfo->UDNHelpString));
		}
		else
		{
			wxLaunchDefaultBrowser(*BaseURLString);
		}

		ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	}
#endif
#endif
}

/** 
 * Event handler for when the "Copy to Clipboard" button is clicked on. Copies the contents
 * of the dialog to the clipboard. If no items are selected, the entire contents are copied.
 * If any items are selected, only selected items are copied.
 */
void WxDlgMapCheck::OnCopyToClipboard(wxCommandEvent& In)
{
	if ( ErrorWarningList->GetItemCount() > 0 )
	{
		// Determine the search flags to use while iterating through the list elements. If anything at all is selected,
		// only consider selected elements; otherwise copy all of the elements to the clipboard.
		const INT SearchState = ( ErrorWarningList->GetSelectedItemCount() > 0 ) ? wxLIST_STATE_SELECTED : wxLIST_STATE_DONTCARE;
		LONG ItemIndex = wxNOT_FOUND;
		ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, SearchState );
		
		// Store an array of the maximum width of each column (saved as # of characters) in order to nicely format
		// each column into the clipboard later.
		const INT ColumnCount = ErrorWarningList->GetColumnCount();
		TArray<INT> MaxColumnWidths( ColumnCount );

		// Initialize the maximum columns widths to zero
		for ( INT ColIndex = 0; ColIndex < MaxColumnWidths.Num(); ++ColIndex )
		{
			MaxColumnWidths(ColIndex) = 0;
		}

		// The current version of wxWidgets appears to only have an awkward way of retrieving data from the list via a
		// query item. The newest version (2.9.?) adds in the ability to query text by column, but for now, data must be queried
		// repeatedly.
		wxListItem QueryItem;
		QueryItem.SetMask( wxLIST_MASK_TEXT );

		// Iterate over each relevant row, storing each string in each column. Ideally this could be done all in place on one
		// FString, but to ensure nice formatting, the maximum length of each column needs to be determined. Rather than iterate
		// twice, just store the strings off in a nested TArray for convenience.
		TArray< TArray<FString> > RowStrings;
		while ( ItemIndex != wxNOT_FOUND )
		{
			const INT RowIndex = RowStrings.AddZeroed();
			TArray<FString>& CurRowStrings = RowStrings(RowIndex);

			// Set the query item to represent the current row
			QueryItem.SetId( ItemIndex );

			// Query for the string in each column of the current row
			for ( INT ColumnIndex = 0; ColumnIndex < ColumnCount; ++ColumnIndex )
			{
				// Set the current column in the query, then ask for the string for this column
				QueryItem.SetColumn( ColumnIndex );
				ErrorWarningList->GetItem( QueryItem );

				const wxString& QueryItemText = QueryItem.GetText();
				const INT QueryItemTexLen = QueryItemText.Len();

				CurRowStrings.AddItem( FString( QueryItemText ) );

				// Check if the current column string is the new maximum width for this column index
				if ( QueryItemTexLen > MaxColumnWidths(ColumnIndex) )
				{
					MaxColumnWidths(ColumnIndex) = QueryItemTexLen;
				}
			}

			// Retrieve the next row's index
			ItemIndex = ErrorWarningList->GetNextItem( ItemIndex, wxLIST_NEXT_ALL, SearchState );
		}

		FString ErrorTextToCopy;

		// Iterate over every relevant string, formatting them nicely into one large FString to copy to the clipboard
		for ( TArray< TArray<FString> >::TConstIterator RowStringIter( RowStrings ); RowStringIter; ++RowStringIter )
		{
			const TArray<FString>& CurRowStrings = *RowStringIter;
			check( CurRowStrings.Num() == ColumnCount );

			for ( TArray<FString>::TConstIterator ColumnIter( CurRowStrings ); ColumnIter; ++ColumnIter )
			{
				// Format the string such that it takes up space equal to the maximum width of its respective column and
				// is left-justified
				if ( ColumnIter.GetIndex() < ColumnCount - 1 )
				{
					ErrorTextToCopy += FString::Printf( TEXT("%-*s"), MaxColumnWidths( ColumnIter.GetIndex() ), **ColumnIter );
					ErrorTextToCopy += TEXT("\t");
				}
				// In the case of the last column, just append the text, it doesn't need any special formatting
				else
				{
					ErrorTextToCopy += *ColumnIter;
				}
			}
			ErrorTextToCopy += TEXT("\r\n");
		}
		appClipboardCopy( *ErrorTextToCopy );
	}
}

/** Event handler for when wx wants to update UI elements. */
void WxDlgMapCheck::OnUpdateUI(wxUpdateUIEvent& In)
{
	switch(In.GetId())
	{
		// Disable these buttons if nothing is selected.
	case IDPB_SHOWHELPPAGE:case IDPB_GOTOACTOR: case IDPB_DELETEACTOR: case IDPB_GOTOPROPERTIES:
		{
			const UBOOL bItemSelected = ErrorWarningList->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED ) != -1;
			In.Enable(bItemSelected == TRUE);
		}
		break;
	}
}

/** Event handler for when the user changes the check status of the filtering categories */
void WxDlgMapCheck::OnFilter(wxCommandEvent& In)
{
	ListFilter[MCTYPE_CRITICALERROR] = CriticalErrorCheckBox->IsChecked();
	ListFilter[MCTYPE_ERROR] = ErrorCheckBox->IsChecked();
	ListFilter[MCTYPE_PERFORMANCEWARNING] = PerfWarningCheckBox->IsChecked();
	ListFilter[MCTYPE_WARNING] = WarningCheckBox->IsChecked();
	ListFilter[MCTYPE_NOTE] = NoteCheckBox->IsChecked();
	ListFilter[MCTYPE_KISMET] = KismetCheckBox->IsChecked();
	ListFilter[MCTYPE_INFO] = InfoCheckBox->IsChecked();

	LoadListCtrl();
}

/** Event handler for when the filtering check boxes need updating/redrawing */
void WxDlgMapCheck::OnUpdateFilterUI(wxUpdateUIEvent& In)
{
	OnSetFilterCheckBoxes();
}

/** Set filtering checkbox states based on the ListFilter member variable */
void WxDlgMapCheck::OnSetFilterCheckBoxes()
{
	CriticalErrorCheckBox->SetValue(TRUE == ListFilter[MCTYPE_CRITICALERROR]);
	ErrorCheckBox->SetValue(TRUE == ListFilter[MCTYPE_ERROR]);
	PerfWarningCheckBox->SetValue(TRUE == ListFilter[MCTYPE_PERFORMANCEWARNING]);
	WarningCheckBox->SetValue(TRUE == ListFilter[MCTYPE_WARNING]);
	NoteCheckBox->SetValue(TRUE == ListFilter[MCTYPE_NOTE]);
	KismetCheckBox->SetValue(TRUE == ListFilter[MCTYPE_KISMET]);
	InfoCheckBox->SetValue(TRUE == ListFilter[MCTYPE_INFO]);
}

/** Set up the columns for the ErrorWarningList control. */
void WxDlgMapCheck::SetupErrorWarningListColumns()
{
	if (ErrorWarningList)
	{
		// Columns are, from left to right: Type, Level, Actor, Message.
		ErrorWarningList->InsertColumn( 0, *LocalizeUnrealEd("Message"), wxLIST_FORMAT_LEFT, 700 );
		ErrorWarningList->InsertColumn( 0, *LocalizeUnrealEd("Actor"), wxLIST_FORMAT_LEFT, 100 );
		ErrorWarningList->InsertColumn( 0, *LocalizeUnrealEd("Level"), wxLIST_FORMAT_LEFT, 100 );
		ErrorWarningList->InsertColumn( 0, *LocalizeUnrealEd("ErrorListItemType"), wxLIST_FORMAT_LEFT, 100 );
	}
}

/** 
 *	Get the level/package name for the given object.
 *
 *	@param	InObject	The object to retrieve the level/package name for.
 *
 *	@return	FString		The name of the level/package.
 */
FString WxDlgMapCheck::GetLevelOrPackageName(UObject* InObject)
{
	AActor* Actor = Cast<AActor>(InObject);
	if (Actor)
	{
		return Actor->GetLevel()->GetOutermost()->GetName();
	}

	return InObject->GetOutermost()->GetName();
}

/** Need a way to get at the virtual GetLevelOrPackageName */
WxDlgMapCheck* DlgMapCheckForCheck = NULL;

/** Compare routine used by sort. */
IMPLEMENT_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckType,
{ 
	INT result = A.Type - B.Type;
	if (0 == result)
	{
		result = appStricmp(*A.Message, *B.Message);
	}
	return result;
});
/** Compare routine used by sort. */
IMPLEMENT_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckLevelName,
{ 
	FString LevelNameA;
	FString LevelNameB;

	if ( A.Object )
	{
		LevelNameA = DlgMapCheckForCheck->GetLevelOrPackageName(A.Object);
	}
	if ( B.Object )
	{
		LevelNameB = DlgMapCheckForCheck->GetLevelOrPackageName(B.Object);
	}
	INT result = appStricmp(*LevelNameA, *LevelNameB);
	if (0 == result)
	{
		result = appStricmp(*A.Message, *B.Message);
	}
	return result;
});

/** Compare routine used by sort. */
IMPLEMENT_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckObjectName,
{ 
	FString ObjectNameA;
	FString ObjectNameB;

	if ( A.Object )
	{
		ObjectNameA = A.Object->GetName();
	}
	if ( B.Object )
	{
		ObjectNameB = B.Object->GetName();
	}
	INT result = appStricmp(*ObjectNameA,*ObjectNameB);
	if (0 == result)
	{
		result = appStricmp(*A.Message, *B.Message);
	}
	return result;
});

/** Compare routine used by sort. */
IMPLEMENT_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckMessage,{ return appStricmp(*A.Message, *B.Message); });

/** Sort based on active column. */
void WxDlgMapCheck::SortErrorWarningInfoList()
{
	DlgMapCheckForCheck = this;
	switch (SortColumn)
	{
		//Level Name
		case 1:
			Sort<USE_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckLevelName)>(ErrorWarningInfoList->GetData(),ErrorWarningInfoList->Num());
			break;
		//Object Name
		case 2:
			Sort<USE_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckObjectName)>(ErrorWarningInfoList->GetData(),ErrorWarningInfoList->Num());
			break;
		//Error Message
		case 3:
			Sort<USE_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckMessage)>(ErrorWarningInfoList->GetData(),ErrorWarningInfoList->Num());
			break;
		case 0:
			// fall through
		default:
			// Sort the error warning info by the TYPE (lowest to highest)
			Sort<USE_COMPARE_CONSTREF(FCheckErrorWarningInfo,DlgMapCheckType)>(ErrorWarningInfoList->GetData(),ErrorWarningInfoList->Num());
			break;
	}
	DlgMapCheckForCheck = NULL;
}
