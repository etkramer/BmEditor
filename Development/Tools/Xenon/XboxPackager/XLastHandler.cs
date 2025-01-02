/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Xml;
using System.Xml.Serialization;

namespace XboxPackager
{
	public class XLastHandler
	{
		private XboxLiveSubmissionProject XLastProject = null;
		private string GameName = "Gear";
		private string ProjectName = "TU1";
		private string ProjectFolder = "GearGame\\TU\\Xbox360\\TU1";
		private string XLastName = "TU";
		private string BuildConfig = "Shipping";
		private int EngineVersion = 0;
		private bool bEnableCompression = false;
		private bool bEnableVerbose = false;

		public XLastHandler()
		{
		}

		public void CaptureOutput( string Message )
		{
			if( bEnableVerbose )
			{
				Log( Message );
			}
		}

		public void Log( string Line, ConsoleColor TextColour )
		{
			if( Line == null )
			{
				return;
			}

			Console.ForegroundColor = TextColour;

			string FullLine = DateTime.Now.ToString( "HH:mm:ss" ) + ": " + Line;

			// Write to the console
			Console.WriteLine( FullLine );
			Console.ResetColor();
		}

		public void Log( string Line )
		{
			Log( Line, Console.ForegroundColor );
		}

		public void Error( string Line )
		{
			Log( "XBP ERROR: " + Line, ConsoleColor.Red );
		}

		public void Warning( string Line )
		{
			Log( "XBP WARNING: " + Line, ConsoleColor.Yellow );
		}

		public bool ParseArguments( string[] Arguments )
		{
			Log( "XboxPackager - a utility to create and build XLAST files", ConsoleColor.White );

			if( Arguments.Length < 2 )
			{
				Error( "Not enough parameters" );
				Log( "XboxPackager.exe <GameName> <XLastName> [/EngineVersion=1234] [/compress] [/verbose] [/buildconfig=Release|Shipping|Test]" );
				return ( false );
			}

			// Set the game name
			GameName = Arguments[0];
			ProjectName = Arguments[1];

			// Set the primary XLast name
			if( XLastName.ToUpper().StartsWith( "TU" ) )
			{
				ProjectFolder = GameName + "Game\\TU\\Xbox360\\" + ProjectName;
				XLastName = ProjectName + ".xlast";
				Log( " ... processing TU: " + ProjectFolder + "\\" + XLastName );
			}
			else if( XLastName.ToUpper().StartsWith( "DLC" ) )
			{
				ProjectFolder = GameName + "Game\\DLC\\Xbox360\\" + ProjectName;
				XLastName = ProjectName + ".xlast";
				Log( " ... processing DLC: " + ProjectFolder + "\\" + XLastName );
			}
			else
			{
				Error( "XLast name must begin with TU or DLC appropriately" );
				return ( false );
			}

			// Find any optional arguments
			for( int ArgumentIndex = 1; ArgumentIndex < Arguments.Length; ArgumentIndex++ )
			{
				string Argument = Arguments[ArgumentIndex];
				if( Argument.ToLower().StartsWith( "/engineversion=" ) )
				{
					string EngineVersionString = Argument.Substring( "/engineversion=".Length );
					EngineVersion = int.Parse( EngineVersionString );
				}
				else if( Argument.ToLower().StartsWith( "/buildconfig=" ) )
				{
					BuildConfig = Argument.Substring( "/buildconfig=".Length );
					if( BuildConfig.ToLower() != "shipping" && BuildConfig.ToLower() != "test" && BuildConfig.ToLower() != "release" )
					{
						Error( "Invalid build configuration (" + BuildConfig + ") - it must be Release, Shipping or Test" );
						return ( false );
					}
				}
				else if( Argument.ToLower().StartsWith( "/compress" ) )
				{
					bEnableCompression = true;
				}
				else if( Argument.ToLower().StartsWith( "/verbose" ) )
				{
					bEnableVerbose = true;
				}
			}

			return( true );
		}

		public bool ReadXLast()
		{
			FileInfo Info = new FileInfo( ProjectFolder + "\\" + XLastName );
			if( !Info.Exists )
			{
				Error( "Could not find XLast file '" + XLastName + "'" );
				return ( false );
			}

			Log( " ... reading: " + XLastName );

			XLastProject = UnrealControls.XmlHandler.ReadXml<XboxLiveSubmissionProject>( ProjectFolder + "\\" + XLastName );
			Info.IsReadOnly = false;

			return ( XLastProject.AutoUpdateProject != null );
		}

		private void RecursiveAddFiles( ref Folder Dir, DirectoryInfo DirInfo )
		{
			foreach( FileInfo Info in DirInfo.GetFiles() )
			{
				FolderFile File = new FolderFile();
				File.Init( Info );
				Dir.Files.Add( File );
			}

			foreach( DirectoryInfo SubDirInfo in DirInfo.GetDirectories() )
			{
				Folder SubDir = new Folder();

				SubDir.Init( SubDirInfo );
				RecursiveAddFiles( ref SubDir, SubDirInfo );

				Dir.Folders.Add( SubDir );
			}
		}

		private void PopulateXLAST( ref XboxUpdateProjectContents Contents, string SourceFolder )
		{
			// Clear out the old folders
			Contents.Folders.Clear();

			// Recursively add all the found folders 
			DirectoryInfo DirInfo = new DirectoryInfo( ProjectFolder + "\\" + SourceFolder );
			if( DirInfo.Exists )
			{
				foreach( DirectoryInfo SubDirInfo in DirInfo.GetDirectories() )
				{
					Folder SubDir = new Folder();

					SubDir.Init( SubDirInfo );
					RecursiveAddFiles( ref SubDir, SubDirInfo );

					Contents.Folders.Add( SubDir );
				}
			}
		}

		private void UpdateTargetVersion( ref XboxAutoUpdateProject AutoProject )
		{
			Version TargetVersion = new Version( AutoProject.UpdateVersion );
			Version NewTargetVersion = new Version( TargetVersion.Major, TargetVersion.Minor, EngineVersion, TargetVersion.Revision );
			AutoProject.UpdateVersion = NewTargetVersion.ToString();
		}

		public bool UpdateXLast()
		{
			Log( " ... updating: " + XLastName );
			
			try
			{
				PopulateXLAST( ref XLastProject.AutoUpdateProject.Contents, "TUExecutable" );

				if( EngineVersion != 0 )
				{
					UpdateTargetVersion( ref XLastProject.AutoUpdateProject );
				}
			}
			catch( Exception Ex )
			{
				Error( "Exception while updating XLast project: " + Ex.ToString() );
				return ( false );
			}

			return ( true );
		}

		public bool WriteXLast()
		{
			Log( " ... writing: " + XLastName );
			UnrealControls.XmlHandler.WriteXml<XboxLiveSubmissionProject>( XLastProject, ProjectFolder + "\\" + XLastName, "http://www.w3.org/2001/XMLSchema-instance" );

			return ( true );
		}

		private string GetBlastPath()
		{
			string XEDKLocation = Environment.GetEnvironmentVariable( "XEDK" );
			if( XEDKLocation == null )
			{
				Error( "Failed to find XEDK environment variable." );
				return ( null );
			}

			string BlastPath = Path.Combine( XEDKLocation, "bin\\win32\\blast.exe" );
			return ( BlastPath );
		}

		private void KillProcess( string ProcessName )
		{
			try
			{
				Process[] ChildProcesses = Process.GetProcessesByName( ProcessName );
				foreach( Process ChildProcess in ChildProcesses )
				{
					Warning( "Killing: '" + ChildProcess.ProcessName + "'" );
					ChildProcess.Kill();
				}
			}
			catch
			{
				Error( "Killing failed" );
			}
		}

		private void CleanDirectory( string DirName )
		{
			DirectoryInfo DirInfo = new DirectoryInfo( ProjectFolder + "\\" + DirName );
			if( DirInfo.Exists )
			{
				DirInfo.Delete( true );
			}
			DirInfo.Create();
		}

		private void CleanFile( string FileName )
		{
			FileInfo Info = new FileInfo( ProjectFolder + "\\" + FileName );
			if( Info.Exists )
			{
				Info.IsReadOnly = false;
				Info.Delete();
			}
		}

		private string GetTUName()
		{
			Version Base = new Version( XLastProject.AutoUpdateProject.BaseVersion );
			string TUName = "tu" + Base.Major.ToString() + Base.Minor.ToString() + Base.Build.ToString( "x4" ) + Base.Revision.ToString( "x2" ) + "_00000000";
			return ( TUName );
		}

		private string GetXexName()
		{
			string XexName = "Binaries\\Xbox360\\" + GameName + "Game-Xbox360-";
			switch( BuildConfig.ToLower() )
			{
			case "shipping":
				XexName += "Shipping";
				break;

			case "release":
				XexName += "Release";
				break;

			case "test":
				XexName += "Test";
				break;
			}

			XexName += ".xex";
			return ( XexName );
		}

		public bool PrepareBuild()
		{
			string XexName = GetXexName();
			FileInfo XexInfo = new FileInfo( XexName );
			if( !XexInfo.Exists )
			{
				Error( "Failed to find xex (" + XexName + ")" );
				return ( false );
			}

			// FIXME: Ought to be default.xex eventually
			FileInfo DestXexInfo = new FileInfo( ProjectFolder + "\\TUExecutable\\GearGame-Xbox360-Shipping.xex" );
			if( DestXexInfo.Exists )
			{
				DestXexInfo.IsReadOnly = false;
			}

			XexInfo.CopyTo( DestXexInfo.FullName, true );
			return ( true );
		}

		public bool CleanBuild()
		{
			Log( "Killing rogue processes ...", ConsoleColor.White );
			KillProcess( "blast" );
			KillProcess( "deltaxex" );

			Log( "Cleaning ...", ConsoleColor.White );
			CleanDirectory( "BuildFiles" );
			CleanDirectory( "Submission" );

			string CabFileName = XLastProject.AutoUpdateProject.TitleId + ".cab";
			CleanFile( CabFileName );
			CleanFile( "titleupdate.xlast" );
			CleanFile( GetTUName() );

			return ( true );
		}

		public bool CreatePackage()
		{
			Log( " ... blasting: " + XLastName );

			string BlastPath = GetBlastPath();
			if( BlastPath == null )
			{
				return ( false );
			}

			Log( " ...... found: " + BlastPath );

			if( UnrealControls.ProcessHandler.SpawnProcessAndWait( BlastPath, ProjectFolder, CaptureOutput, XLastName, "/install:local", bEnableCompression ? "/L:0" : "/L:3" ) == 0 )
			{
				if( UnrealControls.ProcessHandler.SpawnProcessAndWait( BlastPath, ProjectFolder, CaptureOutput, XLastName, "/build" ) == 0 )
				{
					return ( true );
				}
				else
				{
					Error( "Blast /Build failed" );
				}
			}
			else
			{
				Error( "Blast /Install failed" );
			}

			return ( false );
		}

		private bool CopyFileTo( string FileName, string DestDir, bool bMove )
		{
			FileInfo Info = new FileInfo( FileName );
			if( !Info.Exists )
			{
				Error( "Could not find file (" + FileName + ")" );
				return ( false );
			}

			if( bMove )
			{
				Info.MoveTo( Path.Combine( DestDir, Info.Name ) );
			}
			else
			{
				Info.CopyTo( Path.Combine( DestDir, Info.Name ) );
			}

			return ( true );
		}

		public bool RearrangeFiles()
		{
			// Was a TU package created?
			FileInfo TUInfo = new FileInfo( Path.Combine( ProjectFolder + "\\BuildFiles\\Dest\\", GetTUName() ) );
			if( !TUInfo.Exists)
			{
				Error( "Could not find TU file (" + GetTUName() + ")" );
				return ( false );
			}

			FileInfo XLastInfo = new FileInfo( ProjectFolder + "\\BuildFiles\\Dest\\titleupdate.xlast" );
			if( !XLastInfo.Exists )
			{
				Error( "Could not find updated XLast file (titleupdate.xlast)" );
				return ( false );
			}

			// Was a CAB file created
			string CabFileName = XLastProject.AutoUpdateProject.TitleId + ".cab";
			if( !CopyFileTo( ProjectFolder + "\\" + CabFileName, ProjectFolder + "\\Submission\\", true ) )
			{
				return ( false );
			}

			string XexName = GetXexName();
			if( !CopyFileTo( XexName, ProjectFolder + "\\Submission\\", false ) )
			{
				return ( false );
			}

			XexName = Path.ChangeExtension( XexName, ".xdb" );
			if( !CopyFileTo( XexName, ProjectFolder + "\\Submission\\", false ) )
			{
				return ( false );
			}

			XexName = Path.ChangeExtension( XexName, ".pdb" );
			if( !CopyFileTo( XexName, ProjectFolder + "\\Submission\\", false ) )
			{
				return ( false );
			}

			if( !CopyFileTo( TUInfo.FullName, ProjectFolder, true ) )
			{
				return ( false );
			}

			if( !CopyFileTo( XLastInfo.FullName, ProjectFolder, true ) )
			{
				return ( false );
			}

			return ( true );
		}

		public void Finalise()
		{
			DirectoryInfo DirInfo = new DirectoryInfo( ProjectFolder + "\\BuildFiles" );
			if( DirInfo.Exists )
			{
				DirInfo.Delete( true );
			}

			Log( "XboxPackager completed successfully!", ConsoleColor.Green );
		}
	}
}

