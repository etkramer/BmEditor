/*=============================================================================
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "DlgOpenPackages.h"

PackageTreePath::PackageTreePath( FString InPath )
{
	Path = InPath;
}

BEGIN_EVENT_TABLE(WxDlgOpenPackages, wxDialog)
	EVT_BUTTON( wxID_OK, WxDlgOpenPackages::OnOK )
END_EVENT_TABLE()

WxDlgOpenPackages::WxDlgOpenPackages()
{
	const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_OPENPACKAGES") );
	check( bSuccess );

	PackageTreeCtrl = (wxTreeCtrl*)FindWindow( XRCID( "IDTC_PACKAGES" ) );
	check( PackageTreeCtrl != NULL );

	// Hidden root; Content and CookedPCConsole appear as top-level items (wxTR_HIDE_ROOT)
	PackageTreeCtrl->AddRoot( TEXT("") );

	FString GameDir = appGameDir();

	TArray<FString> DirNames;
	TArray<FString> DirPaths;
	DirNames.AddItem( TEXT("Content") );
	DirPaths.AddItem( GameDir + TEXT("Content") );
	DirNames.AddItem( TEXT("CookedPCConsole") );
	DirPaths.AddItem( GameDir + TEXT("CookedPCConsole") );

	for( INT d = 0; d < DirNames.Num(); ++d )
	{
		const FString& DirName = DirNames(d);
		FString UserDirPath = GFileManager->ConvertAbsolutePathToUserPath(
			*GFileManager->ConvertToAbsolutePath(*DirPaths(d)));

		TArray<FString> PackageFilenames;
		appFindFilesInDirectory( PackageFilenames, *UserDirPath, TRUE, FALSE );

		if( PackageFilenames.Num() == 0 )
		{
			continue;
		}

		wxTreeItemId DirRootID = PackageTreeCtrl->AppendItem(
			PackageTreeCtrl->GetRootItem(), *DirName, -1, -1, new PackageTreePath(UserDirPath) );

		for( INT p = 0; p < PackageFilenames.Num(); ++p )
		{
			FString PkgName = PackageFilenames(p);
			TArray<FString> Chunks;
			PkgName.ParseIntoArray( &Chunks, TEXT("\\"), TRUE );

			// Find the anchor chunk (e.g. "Content" or "CookedPCConsole") to mark the tree root
			INT x = 0;
			UBOOL bFoundAnchor = FALSE;
			FString PathBase;

			for( x = 0; x < Chunks.Num(); ++x )
			{
				if( PathBase.Len() )
				{
					PathBase += TEXT("\\");
				}
				PathBase += Chunks(x);

				if( Chunks(x) == DirName )
				{
					bFoundAnchor = TRUE;
					break;
				}
			}

			if( bFoundAnchor )
			{
				wxTreeItemId ParentID = DirRootID;
				FString FullPath = PathBase;

				x++;
				for( ; x < Chunks.Num(); ++x )
				{
					wxTreeItemIdValue cookie;
					wxTreeItemId child;

					FullPath += TEXT("\\");
					FullPath += Chunks(x);

					child = PackageTreeCtrl->GetFirstChild( ParentID, cookie );

					while( child.IsOk() )
					{
						if( PackageTreeCtrl->GetItemText(child) == *Chunks(x) )
						{
							break;
						}
						child = PackageTreeCtrl->GetNextChild( ParentID, cookie );
					}

					if( !child.IsOk() )
					{
						child = PackageTreeCtrl->AppendItem( ParentID, *Chunks(x), -1, -1, new PackageTreePath(FullPath) );
					}

					ParentID = child;
				}
			}
		}

		PackageTreeCtrl->Expand( DirRootID );
	}

	FLocalizeWindow( this );
}

WxDlgOpenPackages::~WxDlgOpenPackages()
{
	FWindowUtil::SavePosSize( TEXT("DlgOpenPackages"), this );
}

void WxDlgOpenPackages::OnOK( wxCommandEvent& In )
{
	SelectedItems.Empty();

	wxArrayTreeItemIds tids;
	PackageTreeCtrl->GetSelections( tids );

	for( UINT x = 0 ; x < tids.GetCount() ; ++x )
	{
		if( PackageTreeCtrl->IsSelected( tids[x] ) && PackageTreeCtrl->GetItemData( tids[x] ) )
		{
			PackageTreePath* Path = (PackageTreePath*)PackageTreeCtrl->GetItemData( tids[x] );
			if( Path->Path.Len() )
			{
				SelectedItems.AddItem( Path->Path );
			}
		}
	}

	EndModal( wxID_OK );
}
