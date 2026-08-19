/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
// BM: Copies content out of an existing game install rather than shipping it in the installer.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using Microsoft.Win32;

namespace UnSetup
{
	public partial class Utils
	{
		// Files copied by ImportContent, relative to the install location
		public List<string> ImportedFiles = new List<string>();

		public bool HasContentImport()
		{
			return ( Manifest != null && Manifest.ContentImport != null && Manifest.ContentImport.SourceFolder.Length > 0 );
		}

		// The folder the content is copied from, given the root of a game install
		public string GetImportSourceFolder( string GameFolder )
		{
			return ( Path.Combine( GameFolder, Manifest.ContentImport.SourceFolder.Replace( '/', '\\' ) ) );
		}

		// A game install is valid if the validation file exists relative to its root
		public bool ValidateGameFolder( string GameFolder )
		{
			if( GameFolder == null || GameFolder.Length == 0 )
			{
				return ( false );
			}

			try
			{
				string ValidationFile = Manifest.ContentImport.ValidationFile.Replace( '/', '\\' );
				return ( new FileInfo( Path.Combine( GameFolder, ValidationFile ) ).Exists );
			}
			catch
			{
			}

			return ( false );
		}

		// The user may pick any folder inside the install, so walk up until the validation file is found
		public string ResolveGameFolder( string SelectedFolder )
		{
			try
			{
				DirectoryInfo DirInfo = new DirectoryInfo( SelectedFolder );
				while( DirInfo != null )
				{
					if( ValidateGameFolder( DirInfo.FullName ) )
					{
						return ( DirInfo.FullName );
					}

					DirInfo = DirInfo.Parent;
				}
			}
			catch
			{
			}

			return ( "" );
		}

		private string GetRegistryString( RegistryKey Root, string KeyName, string ValueName )
		{
			try
			{
				RegistryKey Key = Root.OpenSubKey( KeyName );
				if( Key != null )
				{
					string Value = Key.GetValue( ValueName ) as string;
					Key.Close();
					return ( Value );
				}
			}
			catch
			{
			}

			return ( null );
		}

		// Find the Steam library folders listed in libraryfolders.vdf
		private List<string> GetSteamLibraryFolders()
		{
			List<string> Libraries = new List<string>();

			string SteamPath = GetRegistryString( Registry.LocalMachine, "SOFTWARE\\Wow6432Node\\Valve\\Steam", "InstallPath" );
			if( SteamPath == null )
			{
				SteamPath = GetRegistryString( Registry.LocalMachine, "SOFTWARE\\Valve\\Steam", "InstallPath" );
			}

			if( SteamPath == null )
			{
				return ( Libraries );
			}

			Libraries.Add( SteamPath );

			try
			{
				// Both the modern and legacy vdf formats quote the library path as the second token on a line
				string LibraryFile = Path.Combine( SteamPath, "steamapps\\libraryfolders.vdf" );
				if( new FileInfo( LibraryFile ).Exists )
				{
					Regex PathRegex = new Regex( "\"(path|[0-9]+)\"\\s*\"(?<Path>[^\"]+)\"", RegexOptions.IgnoreCase );
					foreach( string Line in File.ReadAllLines( LibraryFile ) )
					{
						Match PathMatch = PathRegex.Match( Line );
						if( PathMatch.Success )
						{
							Libraries.Add( PathMatch.Groups["Path"].Value.Replace( "\\\\", "\\" ) );
						}
					}
				}
			}
			catch
			{
			}

			return ( Libraries );
		}

		// Read the install folder out of a Steam manifest, as it does not have to match the game name
		private string GetSteamAppFolder( string Library, int AppId )
		{
			try
			{
				string AppManifest = Path.Combine( Library, "steamapps\\appmanifest_" + AppId.ToString() + ".acf" );
				if( !new FileInfo( AppManifest ).Exists )
				{
					return ( null );
				}

				Regex InstallDirRegex = new Regex( "\"installdir\"\\s*\"(?<Dir>[^\"]+)\"", RegexOptions.IgnoreCase );
				foreach( string Line in File.ReadAllLines( AppManifest ) )
				{
					Match DirMatch = InstallDirRegex.Match( Line );
					if( DirMatch.Success )
					{
						return ( Path.Combine( Library, "steamapps\\common\\" + DirMatch.Groups["Dir"].Value ) );
					}
				}
			}
			catch
			{
			}

			return ( null );
		}

		// Search Steam, the uninstall registry keys and any manifest defined folders for the game
		public string FindGameFolder()
		{
			List<string> Candidates = new List<string>();

			if( Manifest.ContentImport.SteamAppId > 0 )
			{
				foreach( string Library in GetSteamLibraryFolders() )
				{
					Candidates.Add( GetSteamAppFolder( Library, Manifest.ContentImport.SteamAppId ) );
				}

				string SteamApp = "Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App " + Manifest.ContentImport.SteamAppId.ToString();
				Candidates.Add( GetRegistryString( Registry.LocalMachine, "SOFTWARE\\" + SteamApp, "InstallLocation" ) );
				Candidates.Add( GetRegistryString( Registry.LocalMachine, "SOFTWARE\\Wow6432Node\\" + SteamApp, "InstallLocation" ) );
			}

			foreach( string SearchFolder in Manifest.ContentImport.SearchFolders )
			{
				Candidates.Add( SearchFolder );
			}

			foreach( string Candidate in Candidates )
			{
				if( Candidate != null && ValidateGameFolder( Candidate ) )
				{
					return ( Candidate );
				}
			}

			return ( "" );
		}

		private Dictionary<string, string> GetRenameMap()
		{
			Dictionary<string, string> Renames = new Dictionary<string, string>();

			foreach( string Rename in Manifest.ContentImport.RenameFiles )
			{
				string[] Pair = Rename.Split( '=' );
				if( Pair.Length == 2 )
				{
					Renames[Pair[0].ToLower().Replace( '/', '\\' )] = Pair[1];
				}
			}

			return ( Renames );
		}

		private bool IsImportableFile( FileInfo Info )
		{
			foreach( string Extension in Manifest.ContentImport.Extensions )
			{
				if( string.Compare( Info.Extension, "." + Extension.TrimStart( '.' ), true ) == 0 )
				{
					return ( true );
				}
			}

			return ( false );
		}

		private void FindImportableFiles( DirectoryInfo SourceInfo, string RelativeFolder, List<string> Files )
		{
			foreach( FileInfo Info in SourceInfo.GetFiles() )
			{
				if( IsImportableFile( Info ) )
				{
					Files.Add( Path.Combine( RelativeFolder, Info.Name ) );
				}
			}

			// The retail folder is flat, and its subfolders hold audio data and whatever else the user left there
			if( !Manifest.ContentImport.bRecursive )
			{
				return;
			}

			foreach( DirectoryInfo Info in SourceInfo.GetDirectories() )
			{
				FindImportableFiles( Info, Path.Combine( RelativeFolder, Info.Name ), Files );
			}
		}

		// Copy the content out of the game install, applying any renames as we go
		public bool ImportContent( string GameFolder, string InstallFolder )
		{
			ImportedFiles.Clear();

			string SourceFolder = GetImportSourceFolder( GameFolder );
			string DestFolder = Path.Combine( InstallFolder, Manifest.ContentImport.DestFolder.Replace( '/', '\\' ) );

			DirectoryInfo SourceInfo = new DirectoryInfo( SourceFolder );
			if( !SourceInfo.Exists )
			{
				return ( false );
			}

			List<string> Files = new List<string>();
			FindImportableFiles( SourceInfo, "", Files );

			Dictionary<string, string> Renames = GetRenameMap();

			int FileIndex = 0;
			foreach( string RelativeName in Files )
			{
				string DestName = RelativeName;
				string RenamedName = null;
				if( Renames.TryGetValue( RelativeName.ToLower(), out RenamedName ) )
				{
					DestName = RenamedName;
				}

				FileInfo Source = new FileInfo( Path.Combine( SourceFolder, RelativeName ) );
				FileInfo Dest = new FileInfo( Path.Combine( DestFolder, DestName ) );

				UpdateProgressBar( GetPhrase( "PBCopyingContent" ), FileIndex, Files.Count );
				Application.DoEvents();

				try
				{
					Directory.CreateDirectory( Dest.DirectoryName );

					// Skip files an earlier install already copied
					if( !Dest.Exists || Dest.Length != Source.Length )
					{
						if( Dest.Exists )
						{
							Dest.IsReadOnly = false;
							Dest.Delete();
						}

						Source.CopyTo( Dest.FullName );
						new FileInfo( Dest.FullName ).IsReadOnly = false;
					}

					ImportedFiles.Add( Path.Combine( Manifest.ContentImport.DestFolder.Replace( '/', '\\' ), DestName ) );
				}
				catch( Exception Ex )
				{
					string Description = Source.FullName + Environment.NewLine + Environment.NewLine + Ex.Message;
					GenericQuery Query = new GenericQuery( "GQCaptionCopyContentFail", Description, false, "GQCancel", true, "GQOK" );
					Query.ShowDialog();
					return ( false );
				}

				FileIndex++;
			}

			UpdateProgressBar( GetPhrase( "PBCopyingContent" ), Files.Count, Files.Count );
			return ( true );
		}

		// Create the folders that ship with no content of their own
		public void CreateEmptyFolders( string InstallFolder )
		{
			foreach( string Folder in Manifest.EmptyFoldersToCreate )
			{
				try
				{
					Directory.CreateDirectory( Path.Combine( InstallFolder, Folder.Replace( '/', '\\' ) ) );
				}
				catch
				{
				}
			}
		}

		// Add the imported files to the installed manifest so that they are removed on uninstall
		public void AddImportedFilesToManifest( string InstallFolder )
		{
			if( ImportedFiles.Count == 0 )
			{
				return;
			}

			string ManifestPath = Path.Combine( InstallFolder, ManifestFileName );

			try
			{
				FolderProperties RootFolderProperty = ReadXml<FolderProperties>( ManifestPath );

				foreach( string RelativeName in ImportedFiles )
				{
					FileInfo Info = new FileInfo( Path.Combine( InstallFolder, RelativeName ) );
					if( Info.Exists )
					{
						RootFolderProperty.AddFile( new FileProperties( RelativeName, Info.Length ) );
					}
				}

				FileInfo ManifestInfo = new FileInfo( ManifestPath );
				if( ManifestInfo.Exists )
				{
					ManifestInfo.IsReadOnly = false;
				}

				WriteXml<FolderProperties>( ManifestPath, RootFolderProperty );
			}
			catch
			{
			}
		}
	}
}
