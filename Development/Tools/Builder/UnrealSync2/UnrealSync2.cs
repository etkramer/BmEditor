/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Deployment.Application;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.Xml;
using System.Xml.Serialization;

using UnrealSync2.Properties;

namespace UnrealSync2
{
	public enum EUserType
	{
		ContentCreator,
		Programmer,
	}

	public enum ESyncType
	{
		Head,
		LatestGoodCIS,
		LatestBuild,
		LatestPromotedBuild,
		LatestQABuild,
		ArtistSyncGame,
	}

	public enum EResolveType
	{
		None,
		Automatic,					// -am
		SafeAutomatic,				// -as
		SafeAutoWithConflicts		// -af
	}

	public partial class UnrealSync2 : Form
	{
		public bool Ticking = false;
		public bool Restart = false;
		public DataBase DB = null;
		public P4 SCC = null;
		public SettableOptions Options = null;

		/** List of unique servers that the build system handles */
		public List<string> PerforceServers = null;

		/** List of clientspec/branch combinations available on this host */
		public List<BranchSpec> BranchSpecs = null;

		/** List of pending sync operations */
		public ReaderWriterQueue<BranchSpec> PendingSyncs = new ReaderWriterQueue<BranchSpec>();

		/** The default number of entries in the context menu */
		private int DefaultContextMenuItemCount = 0;

		/** A list of icons that make the notify tray animation */
		private Icon[] BusyAnimationIcons = new Icon[]
		{
			Icon.FromHandle( Properties.Resources.Sync_00.GetHicon() ),
			Icon.FromHandle( Properties.Resources.Sync_01.GetHicon() ),
			Icon.FromHandle( Properties.Resources.Sync_02.GetHicon() ),
			Icon.FromHandle( Properties.Resources.Sync_03.GetHicon() ),
			Icon.FromHandle( Properties.Resources.Sync_04.GetHicon() ),
			Icon.FromHandle( Properties.Resources.Sync_05.GetHicon() ),
		};

		/** A list of icons that make the notify tray animation */
		private Icon[] ErrorAnimationIcons = new Icon[]
 		{
 			Icon.FromHandle( Properties.Resources.Sync_00.GetHicon() ),
 			Icon.FromHandle( Properties.Resources.Sync_Error.GetHicon() ),
 		};

		/** An event to control the sync animation of the notify tray icon */
		private ManualResetEvent SyncingEvent = new ManualResetEvent( false );
		private ManualResetEvent ErrorEvent = new ManualResetEvent( false );

		/** Last time the db was queried for a newly promoted build */
		private DateTime LastDataBasePollTime = DateTime.MinValue;

		/** Handle to LogViewer window; private to allow suppression of multiple windows */
		private BranchConfigure ConfigureBranches = null;
		private ArtistSyncConfigure ConfigureArtistSync = null;
		private LogViewer ViewLogs = null;

		public UnrealSync2()
		{
		}

		private string GetOptionsFileName()
		{
			string BaseDirectory = Application.StartupPath;
			if( ApplicationDeployment.IsNetworkDeployed )
			{
				BaseDirectory = ApplicationDeployment.CurrentDeployment.DataDirectory;
			}

			string FullPath = Path.Combine( BaseDirectory, "UnrealSync2.Settings.xml" );
			return ( FullPath );
		}

		private ToolStripMenuItem CreateSyncMenuItem( BranchSpec Branch, string Text, string SyncType )
		{
			ToolStripMenuItem MenuItem = new System.Windows.Forms.ToolStripMenuItem();
			MenuItem.Name = SyncType;
			MenuItem.Size = new System.Drawing.Size( 152, 22 );
			MenuItem.Text = Text;
			MenuItem.Checked = false;
			MenuItem.Click += new System.EventHandler( MenuItemClick );
			MenuItem.Tag = ( object )Branch;

			return ( MenuItem );
		}

		private ToolStripMenuItem CreateBranchMenuItem( BranchSpec Branch )
		{
			ToolStripMenuItem MenuItem = new System.Windows.Forms.ToolStripMenuItem();
			MenuItem.Name = Branch.ToString() + "ContextMenuItem";
			MenuItem.Size = new System.Drawing.Size( 152, 22 );
			MenuItem.Text = Branch.Name;
			MenuItem.Checked = false;

			return ( MenuItem );
		}

		private ToolStripMenuItem CreateClientSpecMenuItem( string Name )
		{
			ToolStripMenuItem ClientSpec = new System.Windows.Forms.ToolStripMenuItem();
			ClientSpec.Name = Name + "ContextMenuItem";
			ClientSpec.Size = new System.Drawing.Size( 152, 22 );
			ClientSpec.Text = Name;

			return( ClientSpec );
		}

		private void AddBranches( ToolStripMenuItem ClientSpecMenuItem )
		{
			foreach( BranchSpec Branch in BranchSpecs )
			{
				if( Branch.bDisplayInMenu && Branch.ClientSpec == ClientSpecMenuItem.Text )
				{
					Branch.MenuItem = CreateBranchMenuItem( Branch );

					// Clear out the old options
					Branch.MenuItem.DropDownItems.Clear();

					// Add in the new options depending on the user type
					switch( Options.SyncUserType )
					{
					case EUserType.Programmer:
						AddProgrammerSyncTypes( Branch );
						break;

					case EUserType.ContentCreator:
						AddContentCreatorSyncTypes( Branch );
						break;
					}

					if( Branch.MenuItem.DropDown.Items.Count > 0 )
					{
						ClientSpecMenuItem.DropDownItems.Add( Branch.MenuItem );
					}
				}
			}
		}

		public List<string> GetUniqueClientSpecs( bool bInclusive )
		{
			List<string> UniqueClientSpecs = new List<string>();

			foreach( BranchSpec Branch in BranchSpecs )
			{
				if( bInclusive || Branch.bDisplayInMenu )
				{
					if( !UniqueClientSpecs.Contains( Branch.ClientSpec ) )
					{
						UniqueClientSpecs.Add( Branch.ClientSpec );
					}
				}
			}

			return ( UniqueClientSpecs );
		}

		private void CreateContextMenu()
		{
			UnrealSyncContextMenu.SuspendLayout();

			// Clear out any existing branch options
			while( UnrealSyncContextMenu.Items.Count > DefaultContextMenuItemCount )
			{
				UnrealSyncContextMenu.Items.RemoveAt( 0 );
			}

			// Find and create unique clientspecs
			List<string> UniqueClientSpecs = GetUniqueClientSpecs( false );

			// Create a entry in the context menu for the clientspec
			foreach( string ClientSpec in UniqueClientSpecs )
			{
				ToolStripMenuItem ClientSpecMenuItem = CreateClientSpecMenuItem( ClientSpec );
				AddBranches( ClientSpecMenuItem );

				if( ClientSpecMenuItem.DropDownItems.Count > 0 )
				{
					UnrealSyncContextMenu.Items.Insert( 0, ClientSpecMenuItem );
				}
			}

			UnrealSyncContextMenu.ResumeLayout( true );
		}

		private void AddProgrammerSyncTypes( BranchSpec Branch )
		{
			Branch.MenuItem.DropDown.Items.Add( CreateSyncMenuItem( Branch, "Sync to head", ESyncType.Head.ToString() ) );

			if( Branch.bCISAvailable )
			{
				Branch.MenuItem.DropDown.Items.Add( CreateSyncMenuItem( Branch, "Sync latest good CIS", ESyncType.LatestGoodCIS.ToString() ) );
			}

			Branch.MenuItem.DropDown.Items.Add( CreateSyncMenuItem( Branch, "Sync latest build", ESyncType.LatestBuild.ToString() ) );

			if( Branch.bIsMain )
			{
				Branch.MenuItem.DropDown.Items.Add( CreateSyncMenuItem( Branch, "Sync latest promoted build", ESyncType.LatestPromotedBuild.ToString() ) );
				Branch.MenuItem.DropDown.Items.Add( CreateSyncMenuItem( Branch, "Sync latest QA build", ESyncType.LatestQABuild.ToString() ) );
			}

			Branch.MenuItem.DropDown.Items.Add( "-" );
			Branch.MenuItem.DropDown.Items.Add( CreateSyncMenuItem( Branch, "Explore", "Explore" ) );
		}

		private void AddContentCreatorSyncTypes( BranchSpec Branch )
		{
			string BranchRoot = Path.Combine( Branch.Root, Branch.Name );
			DirectoryInfo DirInfo = new DirectoryInfo( BranchRoot );
			if( DirInfo.Exists )
			{
				foreach( PromotableGame PromotableGame in Branch.PromotableGames )
				{
					ToolStripMenuItem Item = CreateSyncMenuItem( Branch, PromotableGame.GameName + " Artist Sync", ESyncType.ArtistSyncGame.ToString() );
					Branch.MenuItem.DropDown.Items.Add( Item );
				}
			}
		}

		private void ShowBalloon()
		{
			if( SCC.SummaryTitle.Length > 0 )
			{
				UnrealSync2NotifyIcon.BalloonTipTitle = SCC.SummaryTitle;
				UnrealSync2NotifyIcon.BalloonTipText = SCC.SummaryText;
				UnrealSync2NotifyIcon.BalloonTipIcon = SCC.SummaryIcon;
				UnrealSync2NotifyIcon.ShowBalloonTip( 1000 );
			}
		}

		private void HandlePreSyncBat( string BranchRoot )
		{
			if( Options.PreSyncBatFile.Length > 0 )
			{
				UnrealControls.ProcessHandler.SpawnProcessAndWait( Options.PreSyncBatFile, BranchRoot, null );
			}
		}

		private void CleanFiles( BranchSpec Branch, List<string> FilePatterns )
		{
			foreach( string FilePattern in FilePatterns )
			{
				try
				{
					string FullPath = Path.Combine( Path.Combine( Branch.Root, Branch.Name ), FilePattern );
					string Folder = Path.GetDirectoryName( FullPath );
					string FileSpec = Path.GetFileName( FullPath );

					DirectoryInfo DirInfo = new DirectoryInfo( Folder );
					if( DirInfo.Exists )
					{
						FileInfo[] FileInfos = DirInfo.GetFiles( FileSpec );
						foreach( FileInfo Info in FileInfos )
						{
							Info.IsReadOnly = false;
							Info.Delete();
						}
					}
				}
				catch
				{
				}
			}
		}

		private void HandleSync( BranchSpec Branch )
		{
			ESyncType SyncType = Branch.SyncType;
			string GameName = Branch.GameName;
			if( Branch.bImmediateSync )
			{
				SyncType = Branch.ImmediateSyncType;
			}

			switch( SyncType )
			{
			case ESyncType.Head:
				SCC.SyncRevision( Branch, "#head" );
				break;

			case ESyncType.LatestGoodCIS:
				int LatestGoodCIS = DB.GetCISVariable( "LastGoodOverall", Branch.Name );
				if( LatestGoodCIS > 0 )
				{
					SCC.SyncRevision( Branch, "@" + LatestGoodCIS.ToString() );
				}
				break;

			case ESyncType.LatestBuild:
				string LatestBuild = DB.GetBuild( "LatestBuild", Branch.Name );
				if( LatestBuild.Length > 0 )
				{
					SCC.SyncRevision( Branch, "@" + LatestBuild );
				}
				break;

			case ESyncType.LatestPromotedBuild:
				string LatestPromotedBuild = DB.GetBuild( "LatestApprovedBuild", Branch.Name );
				if( LatestPromotedBuild.Length > 0 )
				{
					SCC.SyncRevision( Branch, "@" + LatestPromotedBuild );
				}
				break;

			case ESyncType.LatestQABuild:
				string LatestQABuild = DB.GetBuild( "LatestApprovedQABuild", Branch.Name );
				if( LatestQABuild.Length > 0 )
				{
					SCC.SyncRevision( Branch, "@" + LatestQABuild );
				}
				break;

			case ESyncType.ArtistSyncGame:
				string RulesFileName = GameName + "Game\\Build\\ArtistSyncRules.xml";
				SCC.SyncFile( Branch, RulesFileName );

				RulesFileName = Path.Combine( Branch.Root, Path.Combine( Branch.Name, RulesFileName ) );
				ArtistSyncRules Rules = UnrealControls.XmlHandler.ReadXml<ArtistSyncRules>( RulesFileName );
				if( Rules.PromotionLabel.Length > 0 )
				{
					CleanFiles( Branch, Rules.FilesToClean );
					SCC.SyncRevision( Branch, "@" + Rules.PromotionLabel, Rules.Rules );
				}
				break;

			default:
				break;
			}

			// Display the notify icon if requested
			if( Branch.bImmediateSync )
			{
				ShowBalloon();
			}
		}

		/** Consumer thread waiting for sync jobs */
		private void ManageSyncs()
		{
			while( Ticking )
			{
				if( PendingSyncs.Count > 0 )
				{
					SyncingEvent.Reset();

					BranchSpec Branch = PendingSyncs.Dequeue();
					HandlePreSyncBat( Path.Combine( Branch.Root, Branch.Name ) );
					HandleSync( Branch );

					SetOptions();
					UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, GetOptionsFileName(), "" );

					SyncingEvent.Set();

					if( SCC.SummaryIcon == ToolTipIcon.Error )
					{
						ErrorEvent.Reset();
					}
				}

				Thread.Sleep( 1000 );
			}
		}

		/** 
		 * Store a list of branches that we wish to display in the popup menu in the settings file
		 */
		private void SetOptions()
		{
			Options.Branches.Clear();

			foreach( BranchSpec Branch in BranchSpecs )
			{
				BranchPersistentData BranchInfo = new BranchPersistentData();

				// Set the branch specific data
				BranchInfo.Server = Branch.Server;
				BranchInfo.ClientSpec = Branch.ClientSpec;
				BranchInfo.Name = Branch.Name;
				BranchInfo.bDisplayInMenu = Branch.bDisplayInMenu;
				BranchInfo.SyncType = Branch.SyncType;
				BranchInfo.SyncTime = Branch.SyncTime;

				// Set the game specific data for this branch
				foreach( PromotableGame Game in Branch.PromotableGames )
				{
					BranchInfo.PromotableGames.Add( new PromotableGame( Game ) );
				}

				Options.Branches.Add( BranchInfo );
			}
		}

		public void SetPromotableGameSyncTime( BranchSpec Branch, string GameName, DateTime SyncTime )
		{
			foreach( PromotableGame Game in Branch.PromotableGames )
			{
				if( Game.GameName == GameName )
				{
					Game.SyncTime = SyncTime;
				}
			}
		}

		/**
		 * Apply the persistent data from the settings file to the active data
		 */
		private void ApplyOptions()
		{
			// Work out if any branches are displayed
			bool bBranchDisplayed = false;
			foreach( BranchPersistentData BranchInfo in Options.Branches )
			{
				bBranchDisplayed |= BranchInfo.bDisplayInMenu;
			}

			// Apply stored data to branch config
			foreach( BranchPersistentData BranchInfo in Options.Branches )
			{
				foreach( BranchSpec Branch in BranchSpecs )
				{
					if( Branch.Server == BranchInfo.Server && Branch.ClientSpec == BranchInfo.ClientSpec && Branch.Name == BranchInfo.Name )
					{
						Branch.bDisplayInMenu = BranchInfo.bDisplayInMenu;
						Branch.SyncType = BranchInfo.SyncType;
						Branch.SyncTime = BranchInfo.SyncTime;

						// If no branches displayed in the menu, enable the main branch by default
						if( !bBranchDisplayed )
						{
							Branch.bDisplayInMenu |= Branch.bIsMain;
						}

						// Set the artist sync times from the settings file
						foreach( PromotableGame Game in BranchInfo.PromotableGames )
						{
							SetPromotableGameSyncTime( Branch, Game.GameName, Game.SyncTime );
						}
					}
				}
			}
		}

		/** 
		 * Look for valid ArtistSyncRules.xml files in Perforce for all valid games found on disk
		 */
		private void CachePromotableGames()
		{
			foreach( BranchSpec Branch in BranchSpecs )
			{
				// Clear out the old list of games
				Branch.PromotableGames.Clear();

				// Reinterrogate Perforce for the latest set
				string BranchRoot = Path.Combine( Branch.Root, Branch.Name );
				List<string> ValidGames = UnrealControls.GameLocator.LocateGames( BranchRoot );
				foreach( string ValidGame in ValidGames )
				{
					string ArtistSyncRulesName = ValidGame + "Game/Build/ArtistSyncRules.xml";
					if( SCC.ValidateFile( Branch, ArtistSyncRulesName ) )
					{
						Branch.PromotableGames.Add( new PromotableGame( ValidGame ) );
					}
				}
			}
		}

		/**
		 * Main init function
		 */
		public bool Init( string[] Args )
		{
			Options = UnrealControls.XmlHandler.ReadXml<SettableOptions>( GetOptionsFileName() );

			// Init the database connection
			DB = new DataBase();

			// Init source control
			SCC = new P4( this );

			PerforceServers = DB.GetPerforceServers();

			// Get a list of available clientspecs from Perforce
			BranchSpecs = SCC.GetClientSpecs( PerforceServers );

			// Grab any pertinent info from the database
			DB.PopulateBranchSpecs( this );

			// Cache games with valid ArtistSyncRules.xml files
			CachePromotableGames();

			// Apply any remembered settings to the list of branches
			ApplyOptions();

			// Init the forms
			InitializeComponent();
			UnrealSync2NotifyIcon.Icon = BusyAnimationIcons[0];

			// Create the context menu dynamically
			DefaultContextMenuItemCount = UnrealSyncContextMenu.Items.Count;
			// Add the available clientspecs and the available commands depending on the user's sync type
			CreateContextMenu();

			// Set the configuration grid
			UnrealSyncConfigurationGrid.SelectedObject = Options;

			System.Version Version = System.Reflection.Assembly.GetExecutingAssembly().GetName().Version;
			DateTime CompileDateTime = DateTime.Parse( "01/01/2000" ).AddDays( Version.Build ).AddSeconds( Version.Revision * 2 );
			VersionText.Text = "Compiled: " + CompileDateTime.ToString( "d MMM yyyy HH:mm" );

			Ticking = true;

			// Start the thread that monitors sync to handle
			Thread ManageSyncThread = new Thread( ManageSyncs );
			ManageSyncThread.Start();

			// Start the thread that animates the cursor
			Thread BusyThread = new Thread( BusyThreadProc );
			BusyThread.Start();

			SyncingEvent.Set();
			ErrorEvent.Set();

			return ( true );
		}

		public void Destroy()
		{
			Dispose();

			SetOptions();
			UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, GetOptionsFileName(), "" );
		}

		public void Run()
		{
			/** Check to see if any syncs need spawning */
			CheckSyncSchedule();

			/** Check to see if any builds were promoted */
			CheckBuilds();
		}

		/** Check to see if any syncs need spawning */
		private void CheckSyncSchedule()
		{
			foreach( BranchSpec Branch in BranchSpecs )
			{
				// Check for scheduled main branch syncs
				if( DateTime.Now - Branch.SyncRandomisation > Branch.SyncTime )
				{
					Branch.bImmediateSync = false;
					PendingSyncs.Enqueue( Branch );

					while( DateTime.Now > Branch.SyncTime )
					{
						Branch.SyncTime = Branch.SyncTime.AddDays( 1.0 );
					}
					Branch.SyncRandomisation = new TimeSpan( 0, new Random().Next( 30 ), 0 );
				}

				// Check for scheduled artist syncs
				foreach( PromotableGame Game in Branch.PromotableGames )
				{
					if( DateTime.Now - Game.SyncRandomisation > Game.SyncTime )
					{
						Branch.bImmediateSync = false;
						Branch.GameName = Game.GameName;
						PendingSyncs.Enqueue( Branch );

						while( DateTime.Now > Game.SyncTime )
						{
							Game.SyncTime = Game.SyncTime.AddDays( 1.0 );
						}
						Game.SyncRandomisation = new TimeSpan( 0, new Random().Next( 30 ), 0 );
					}
				}
			}
		}

		private void DisplayNewBuild( BranchSpec Branch, ref List<string> Processed, string Title, string Content, string Label, bool bPlaySound )
		{
			if( !Processed.Contains( Branch.Name ) )
			{
				UnrealSync2NotifyIcon.BalloonTipTitle = Title;
				UnrealSync2NotifyIcon.BalloonTipText = Branch.Name + Content + Environment.NewLine + Label;
				UnrealSync2NotifyIcon.BalloonTipIcon = ToolTipIcon.Info;
				UnrealSync2NotifyIcon.ShowBalloonTip( 1000 );

				if( bPlaySound )
				{
					System.Media.SystemSounds.Exclamation.Play();
				}

				Processed.Add( Branch.Name );
			}
		}

		/** Check to see if any builds were promoted */
		private void CheckBuilds()
		{
			if( LastDataBasePollTime < DateTime.Now )
			{
				LastDataBasePollTime = DateTime.Now.AddSeconds( Options.PollingInterval );

				List<string> PromotedBranchNames = new List<string>();
				List<string> PoppedBranchNames = new List<string>();

				foreach( BranchSpec Branch in BranchSpecs )
				{
					// Check for new QA build
					string LatestApprovedQABuild = DB.GetBuild( "LatestApprovedQABuild", Branch.Name );
					if( LatestApprovedQABuild != Branch.LatestQABuild )
					{
						if( Options.ShowPromotions )
						{
							DisplayNewBuild( Branch, ref PromotedBranchNames, "New QA build!", " was promoted to build label", LatestApprovedQABuild, Options.PlaySoundOnPromotion );
						}
						Branch.LatestQABuild = LatestApprovedQABuild;
					}

					// Check for newly promoted builds
					string LatestApprovedBuild = DB.GetBuild( "LatestApprovedBuild", Branch.Name );
					if( LatestApprovedBuild != Branch.LatestApprovedBuild )
					{
						if( Options.ShowPromotions )
						{
							DisplayNewBuild( Branch, ref PromotedBranchNames, "New build promoted!", " was promoted to build label", LatestApprovedBuild, Options.PlaySoundOnPromotion );
						}
						Branch.LatestApprovedBuild = LatestApprovedBuild;
					}

					// Check for newly popped builds
					string LatestBuild = DB.GetBuild( "LatestBuild", Branch.Name );
					if( LatestBuild != Branch.LatestBuild )
					{
						if( Options.ShowBuildPops )
						{
							DisplayNewBuild( Branch, ref PoppedBranchNames, "New build!", " has a new build", LatestBuild, Options.PlaySoundOnBuildPop );
						}
						Branch.LatestBuild = LatestBuild;
					}

					// Update the CIS info
					if( Branch.bCISAvailable )
					{
						Branch.LastGoodCIS = DB.GetCISVariable( "LastGoodOverall", Branch.Name ).ToString();
						Branch.HeadChangelist = DB.GetCISVariable( "LastAttemptedOverall", Branch.Name ).ToString();
					}
				}
			}
		}

		/** Thread to handle animating the tray icon */
		private void BusyThreadProc( object Data )
		{
			while( Ticking )
			{
				int AnimationIndex = 0;
				while( Ticking && !SyncingEvent.WaitOne( 66 ) )
				{
					Icon[] Animation = BusyAnimationIcons;
					AnimationIndex++;
					if( AnimationIndex >= Animation.Length )
					{
						AnimationIndex = 0;
					}

					UnrealSync2NotifyIcon.Icon = Animation[AnimationIndex];
				}

				while( Ticking && !ErrorEvent.WaitOne( 132 ) )
				{
					Icon[] Animation = ErrorAnimationIcons;
					AnimationIndex++;
					if( AnimationIndex >= Animation.Length )
					{
						AnimationIndex = 0;
					}

					UnrealSync2NotifyIcon.Icon = Animation[AnimationIndex];
				}

				if( Ticking )
				{
					UnrealSync2NotifyIcon.Icon = BusyAnimationIcons[0];
				}

				Thread.Sleep( 100 );
			}
		}

		/** Start the process of quitting the application */
		private void ExitMenuItemClick( object sender, EventArgs e )
		{
			Ticking = false;
		}

		private void MenuItemClick( object sender, EventArgs e )
		{
			ToolStripMenuItem MenuItem = ( ToolStripMenuItem )sender;
			BranchSpec Branch = ( BranchSpec )MenuItem.Tag;

			switch( MenuItem.Name )
			{
			case "Explore":
				Process.Start( Path.Combine( Branch.Root, Branch.Name ) );
				break;

			default:
				ESyncType SyncType = ( ESyncType )Enum.Parse( typeof( ESyncType ), MenuItem.Name, false );

				// Set up the game to artist sync
				if( SyncType == ESyncType.ArtistSyncGame )
				{
					string[] Params = MenuItem.Text.Split( " ".ToCharArray() );
					if( Params.Length > 0 )
					{
						Branch.GameName = Params[0];
					}
				}

				// Enqueue the job
				Branch.bImmediateSync = true;
				Branch.ImmediateSyncType = SyncType;
				PendingSyncs.Enqueue( Branch );
				break;
			}
		}

		/** Bring up the branch configuration dialog */
		private void ContextMenuConfigureBranchesClicked( object sender, EventArgs e )
		{
			if( ConfigureBranches == null && ConfigureArtistSync == null )
			{
				if( Options.SyncUserType != EUserType.ContentCreator )
				{
					ConfigureBranches = new BranchConfigure( this );
					ConfigureBranches.ShowDialog();
				}
				else
				{
					ConfigureArtistSync = new ArtistSyncConfigure( this );
					ConfigureArtistSync.ShowDialog();
				}
			}
			else
			{
				if( ConfigureBranches != null )
				{
					ConfigureBranches.BranchConfigureOKButtonClicked( null, null );
				}
				else if( ConfigureArtistSync != null )
				{
					ConfigureArtistSync.ArtistSyncConfigureOKButtonClicked( null, null );
				}
			}

			SetOptions();
			CreateContextMenu();
			UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, GetOptionsFileName(), "" );

			ConfigureBranches = null;
			ConfigureArtistSync = null;
		}

		/** Handle the preferences dialog */
		private void ContextMenuPreferencesClicked( object sender, EventArgs e )
		{
			Show();
		}

		/** Launch P4Client */
		private void ContextMenuP4WinClicked( object sender, EventArgs e )
		{
			Process.Start( Options.PerforceGUIClient );
		}

		/** Redisplay the last displayed sync info */
		private void TrayIconClicked( object sender, EventArgs e )
		{
			MouseEventArgs Event = ( MouseEventArgs )e;
			if( Event.Button == MouseButtons.Left )
			{
				ShowBalloon();
			}
		}

		private void ButtonOKClick( object sender, EventArgs e )
		{
			Hide();

			SetOptions();
			CreateContextMenu();
			UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, GetOptionsFileName(), "" );
		}

		/** Bring up the dialog that shows all the logs */
		private void ContextMenuShowLogsClicked( object sender, EventArgs e )
		{
			if( ViewLogs == null )
			{
				ViewLogs = new LogViewer( this );
				ViewLogs.ShowDialog();
			}
			else
			{
				ViewLogs.Close();
			}

			ErrorEvent.Set();
			ViewLogs = null;
		}

		private void BalloonTipClicked( object sender, EventArgs e )
		{
			ContextMenuShowLogsClicked( sender, e );
		}

		public List<DataGridViewCellStyle> GetGridStyles( int Count )
		{
			// Create a cell style with a unique background per clientspec
			List<Color> Colours = new List<Color>() 
			{ 
				Color.Honeydew,
				Color.LavenderBlush, 
				Color.LightCyan,
				Color.LightYellow,
			};

			List<DataGridViewCellStyle> CellStyles = new List<DataGridViewCellStyle>();
			for( int Index = 0; Index < Count; Index++ )
			{
				DataGridViewCellStyle CellStyle = new DataGridViewCellStyle();
				CellStyle.BackColor = Colours[Index % Colours.Count];
				CellStyles.Add( CellStyle );
			}

			return ( CellStyles );
		}

		public DateTime GetSyncTime( string Time )
		{
			DateTime SyncTime = DateTime.MaxValue;
			if( Time != "Never" )
			{
				if( DateTime.TryParse( Time, out SyncTime ) )
				{
					if( SyncTime < DateTime.Now )
					{
						SyncTime = SyncTime.AddDays( 1.0 );
					}
				}
			}

			return ( SyncTime );
		}

		/** 
		 * A class containing the information about a promotable game in a branch
		 */
		public class PromotableGame
		{
			[XmlAttribute]
			public string GameName;
			[XmlAttribute]
			public DateTime SyncTime;
			[XmlIgnore]
			public TimeSpan SyncRandomisation;

			public PromotableGame()
			{
				GameName = "";
				SyncTime = DateTime.MaxValue;
				SyncRandomisation = new TimeSpan( 0, new Random().Next( 30 ), 0 );
			}

			public PromotableGame( string InGameName )
			{
				GameName = InGameName;
				SyncTime = DateTime.MaxValue;
				SyncRandomisation = new TimeSpan( 0, new Random().Next( 30 ), 0 );
			}

			public PromotableGame( PromotableGame Game )
			{
				GameName = Game.GameName;
				SyncTime = Game.SyncTime;
				SyncRandomisation = Game.SyncRandomisation;
			}
		}

		/** 
		 * A class containing the persistent (saveable) information about a branch
		 */
		public class BranchPersistentData
		{
			[XmlAttribute]
			public string Server;
			[XmlAttribute]
			public string ClientSpec;
			[XmlAttribute]
			public string Name;
			[XmlAttribute]
			public bool bDisplayInMenu;
			[XmlAttribute]
			public ESyncType SyncType;
			[XmlAttribute]
			public DateTime SyncTime;
			[XmlElement]
			public List<PromotableGame> PromotableGames;

			public BranchPersistentData()
			{
				Server = "";
				ClientSpec = "";
				Name = "";
				bDisplayInMenu = false;
				SyncType = ESyncType.Head;
				SyncTime = DateTime.MaxValue;
				PromotableGames = new List<PromotableGame>();
			}
		}

		/** 
		 * A class containing the rules to complete an artist sync
		 */
		public class ArtistSyncRules
		{
			[XmlElement]
			public string PromotionLabel;
			[XmlArray]
			public List<string> FilesToClean;
			[XmlArray]
			public List<string> Rules;

			public ArtistSyncRules()
			{
				PromotionLabel = "";
				FilesToClean = new List<string>();
				Rules = new List<string>();
			}
		}

		/** 
		 * A class containing the settable options for a user
		 */
		public class SettableOptions
		{
			[XmlElement]
			public List<BranchPersistentData> Branches = new List<BranchPersistentData>();

			[XmlElement]
			public string DefaultPerforcePort = "p4-server:1666";

			[CategoryAttribute( "Syncing" )]
			[DescriptionAttribute( "Set the type of syncs for your desired workflow." )]
			[XmlElement]
			public EUserType SyncUserType { get; set; }

			[CategoryAttribute( "Syncing" )]
			[DescriptionAttribute( "Set whether to auto clobber (overwrite) files that are locally writable, but not checked out." )]
			[XmlElement]
			public bool AutoClobber { get; set; }

			[CategoryAttribute( "Syncing" )]
			[DescriptionAttribute( "Set the type of resolve to use." )]
			[XmlElement]
			public EResolveType ResolveType { get; set; }

			[CategoryAttribute( "Syncing" )]
			[DescriptionAttribute( "A bat file to run before each sync operation. It is set to run in the root folder of the branch (e.g. \"d:\\depot\\UnrealEngine3\")" )]
			[XmlElement]
			public string PreSyncBatFile { get; set; }

			[CategoryAttribute( "Build Promotion" )]
			[DescriptionAttribute( "Shows the tip balloon when a new build is promoted." )]
			[XmlElement]
			public bool ShowPromotions { get; set; }

			[CategoryAttribute( "Build Promotion" )]
			[DescriptionAttribute( "Play a sound when a new build is promoted." )]
			[XmlElement]
			public bool PlaySoundOnPromotion { get; set; }

			[CategoryAttribute( "Build Popping" )]
			[DescriptionAttribute( "Shows the tip balloon when a new build is created." )]
			[XmlElement]
			public bool ShowBuildPops { get; set; }

			[CategoryAttribute( "Build Popping" )]
			[DescriptionAttribute( "Play a sound when a new build is created." )]
			[XmlElement]
			public bool PlaySoundOnBuildPop { get; set; }

			[CategoryAttribute( "Preferences" )]
			[DescriptionAttribute( "The Perforce GUI client (normally P4Win.exe)" )]
			[XmlElement]
			public string PerforceGUIClient { get; set; }

			[CategoryAttribute( "Preferences" )]
			[DescriptionAttribute( "Polling interval in seconds (default is 120, with clamps at 60 and 300)." )]
			[XmlElement]
			public int PollingInterval
			{
				get
				{
					return ( LocalPollingInterval );
				}
				set
				{
					if( value < 60 )
					{
						value = 60;
					}
					else if( value > 300 )
					{
						value = 300;
					}
					LocalPollingInterval = value;
				}
			}
			[XmlAttribute]
			private int LocalPollingInterval = 120;

			public SettableOptions()
			{
				AutoClobber = false;
				ResolveType = EResolveType.SafeAutomatic;
				PreSyncBatFile = "";
				ShowPromotions = true;
				PlaySoundOnPromotion = true;
				ShowBuildPops = true;
				PlaySoundOnBuildPop = true;
				PerforceGUIClient = "P4Win.exe";
			}
		}
	}
}
