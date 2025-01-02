/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Deployment.Application;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Windows.Forms;
using P4API;

namespace UnrealSync2
{
	public class P4
	{
		public string SummaryTitle = "";
		public string SummaryText = "";
		public ToolTipIcon SummaryIcon = ToolTipIcon.None;

		private int ErrorCount = 0;
		private int WarningCount = 0;
		private int MessageCount = 0;
		private int FilesSynced = 0;
		private TimeSpan TotalDuration = TimeSpan.MinValue;

		private UnrealSync2 Parent = null;
		private P4Connection IP4Net = null;
		private TextWriter Log = null;

		public P4( UnrealSync2 InParent )
		{
			Parent = InParent;
			IP4Net = new P4Connection();
		}

		private void DisplayPerforceError( string Message )
		{
#if DEBUG
			Debug.WriteLine( Message );
#endif
			if( Message.Contains( "Access for user" ) )
			{
				System.Windows.Forms.MessageBox.Show( "Perforce user name \"" + IP4Net.User + "\" not recognised by server \"" + IP4Net.Port.ToString() + "\". Contact your Perforce administrator.",
													  "UnrealSync2: Perforce Connection Error",
													  System.Windows.Forms.MessageBoxButtons.OK,
													  System.Windows.Forms.MessageBoxIcon.Error );
			}
			else if( Message.Contains( "Perforce password" ) )
			{
				System.Windows.Forms.MessageBox.Show( "Incorrect Password for \"" + IP4Net.User + "\" for server \"" + IP4Net.Port.ToString() + "\". Contact your Perforce administrator.",
													  "UnrealSync2: Perforce Connection Error",
													  System.Windows.Forms.MessageBoxButtons.OK,
													  System.Windows.Forms.MessageBoxIcon.Error );
			}
			else if( Message.Contains( "Unable to connect to the Perforce Server!" ) )
			{
				System.Windows.Forms.MessageBox.Show( "Unable to connect user \"" + IP4Net.User + "\" to the default Perforce Server \"" + IP4Net.Port.ToString() + "\". Contact your Perforce administrator.",
													  "UnrealSync2: Perforce Connection Error",
													  System.Windows.Forms.MessageBoxButtons.OK,
													  System.Windows.Forms.MessageBoxIcon.Error );
			}
		}

		public bool IsLoggedIn( string PerforceServer )
		{
			bool bIsLoggedIn = false;
			try
			{
				// Try to auto login
				IP4Net.Port = PerforceServer;
				IP4Net.CWD = "";
				IP4Net.ExceptionLevel = P4API.P4ExceptionLevels.ExceptionOnBothErrorsAndWarnings;

				P4RecordSet Output = IP4Net.Run( "login", "-s" );
				bIsLoggedIn = true;
			}
			catch( Exception Ex )
			{
				if( PerforceServer == Parent.Options.DefaultPerforcePort )
				{
					DisplayPerforceError( Ex.Message );
				}
			}

			IP4Net.Disconnect();
			return ( bIsLoggedIn );
		}

		// Sync a single file
		public void SyncFile( BranchSpec Branch, string FileName )
		{
			try
			{
				IP4Net.Port = Branch.Server;
				IP4Net.Client = Branch.ClientSpec;
				IP4Net.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;
				string Parameters = "//depot/" + Branch.Name + "/" + FileName;
				IP4Net.Run( "sync", Parameters );
			}
			catch
			{
			}

			IP4Net.Disconnect();
		}

		// Test whether a file exists in Perforce under this branch
		public bool ValidateFile( BranchSpec Branch, string FileName )
		{
			P4RecordSet Results = null;
			try
			{
				IP4Net.Port = Branch.Server;
				IP4Net.Client = Branch.ClientSpec;
				IP4Net.ExceptionLevel = P4ExceptionLevels.ExceptionOnBothErrorsAndWarnings;
				string Parameters = "//depot/" + Branch.Name + "/" + FileName;
				// This will exception if the file does not exist
				Results = IP4Net.Run( "fstat", Parameters );
			}
			catch
			{
			}

			IP4Net.Disconnect();

			return ( Results != null );
		}

		private bool ValidateBranch( string BranchName )
		{
			// Make sure it includes all files
			if( !BranchName.EndsWith( "/..." ) )
			{
				return( false );
			}

			List<string> BranchFiles = new List<string>()
			{
				"/Binaries/build.properties",
				"/Engine/Config/BaseEngine.ini"
			};

			string BranchRoot = BranchName.Substring( 0, BranchName.Length - "/...".Length );

			foreach( string BranchFile in BranchFiles )
			{
				try
				{
					string BranchFileName = BranchRoot + BranchFile;
					IP4Net.Run( "fstat", BranchFileName );
				}
				catch
				{
					return ( false );
				}
			}

			return ( true );
		}

		private void AnalyseClientSpecs( string P4Server, P4Record ClientSpec, ref List<BranchSpec> BranchSpecs )
		{
			// Grab the details of the clientspec
			string ClientSpecName = ClientSpec["client"];
			IP4Net.Client = ClientSpecName;
			P4RecordSet ClientSpecDetails = IP4Net.Run( "client", "-o", ClientSpecName );

			// Make a list of unique branch references
			if( ClientSpecDetails.Records.Length > 0 && ClientSpecDetails.Records[0].ArrayFields["View"] != null )
			{
				List<string> Branches = new List<string>();
				foreach( string View in ClientSpecDetails.Records[0].ArrayFields["View"] )
				{
					string PotentialBranch = View.ToLower();

					if( PotentialBranch.StartsWith( "-" ) )
					{
						continue;
					}

					string DepotFileSpec = PotentialBranch.Substring( 0, PotentialBranch.IndexOf( " " ) );
					if( !ValidateBranch( DepotFileSpec ) )
					{
						Debug.WriteLine( " ...... ignoring branch: " + PotentialBranch );
						continue;
					}

					// Extract the depot name of the branch
					int StringIndex = PotentialBranch.IndexOf( " " );
					if( StringIndex < 0 )
					{
						continue;
					}
					string BranchDepotName = View.Substring( 0, StringIndex );

					BranchDepotName = BranchDepotName.TrimStart( "/".ToCharArray() );

					StringIndex = BranchDepotName.IndexOf( '/' );
					if( StringIndex < 0 )
					{
						continue;
					}
					BranchDepotName = BranchDepotName.Substring( StringIndex + 1 );

					StringIndex = BranchDepotName.IndexOf( '/' );
					if( StringIndex < 0 )
					{
						continue;
					}
					BranchDepotName = BranchDepotName.Substring( 0, StringIndex );

					// Get the client name of the branch
					int ClientSpecIndex = PotentialBranch.IndexOf( ClientSpecName.ToLower() );
					if( ClientSpecIndex < 0 )
					{
						continue;
					}
					PotentialBranch = View.Substring( ClientSpecIndex + ClientSpecName.Length + 1 );

					StringIndex = PotentialBranch.IndexOf( '/' );
					if( StringIndex < 0 )
					{
						continue;
					}
					string BranchName = PotentialBranch.Substring( 0, StringIndex );

					// If we haven't already, add it to the list
					if( !Branches.Contains( BranchName ) )
					{
						string BranchRoot = ClientSpec["Root"];

						BranchSpec NewBranch = new BranchSpec( P4Server, ClientSpecName, BranchName, BranchDepotName, BranchRoot );
						BranchSpecs.Add( NewBranch );

						Branches.Add( BranchName );
						Debug.WriteLine( " ...... added branch: " + NewBranch.ToString() );
					}
				}
			}
		}

		public List<BranchSpec> GetClientSpecs( List<string> PerforceServers )
		{
			List<BranchSpec> BranchSpecs = new List<BranchSpec>();

			Debug.WriteLine( "Analysing servers ..." );
			foreach( string P4Server in PerforceServers )
			{
				Debug.WriteLine( " ... analysing clientspecs for: " + P4Server );
				if( IsLoggedIn( P4Server ) )
				{
					// Get a list of clientspecs associated with this user and server
					IP4Net.Port = P4Server;
					P4RecordSet ClientSpecs = IP4Net.Run( "clients", "-u", IP4Net.User );

					// Find out which ones are on this machine
					foreach( P4Record ClientSpec in ClientSpecs.Records )
					{
						// Check to see if we're on the correct host
						if( ClientSpec["Host"].ToLower() == Environment.MachineName.ToLower() )
						{
							AnalyseClientSpecs( P4Server, ClientSpec, ref BranchSpecs );
						}
						else if( ClientSpec["Host"].Length == 0 )
						{
							// ... or if we have a blank host
							string BranchRoot = ClientSpec["Root"];
							if( Directory.Exists( BranchRoot ) )
							{
								AnalyseClientSpecs( P4Server, ClientSpec, ref BranchSpecs );
							}
						}
						else
						{
							Debug.WriteLine( " ...... ignoring clientspec (wrong host): " + ClientSpec["client"] );
						}
					}

					IP4Net.Disconnect();
				}
				else
				{
					Debug.WriteLine( " ...... not logged in" );
				}
			}

			return ( BranchSpecs );
		}

		private void OpenLog( BranchSpec Branch )
		{
			DateTime LocalTime = DateTime.Now;

			string TimeStamp = LocalTime.Year + "-"
						+ LocalTime.Month.ToString( "00" ) + "-"
						+ LocalTime.Day.ToString( "00" ) + "_"
						+ LocalTime.Hour.ToString( "00" ) + "."
						+ LocalTime.Minute.ToString( "00" ) + "."
						+ LocalTime.Second.ToString( "00" );

			string BaseDirectory = Application.StartupPath;
			if( ApplicationDeployment.IsNetworkDeployed )
			{
				BaseDirectory = ApplicationDeployment.CurrentDeployment.DataDirectory;
			}

			string LogFileName = "[" + TimeStamp + "]_" + Branch.Name + ".txt";
			string LogPath = Path.Combine( BaseDirectory, LogFileName );

			Log = TextWriter.Synchronized( new StreamWriter( LogPath, true, Encoding.Unicode ) );

			WriteToLog( "ClientSpec: " + Branch.ClientSpec + " (" + Branch.Root + ")" );
			WriteToLog( "" );
			WriteToLog( "AutoClobber: " + Parent.Options.AutoClobber.ToString() );
			WriteToLog( "ResolveType: " + Parent.Options.ResolveType.ToString() );
			WriteToLog( "" );

			ErrorCount = 0;
			WarningCount = 0;
			MessageCount = 0;
			FilesSynced = 0;
			TotalDuration = new TimeSpan( 0 );
		}

		private void AnalyseResults()
		{
			if( ErrorCount > 0 )
			{
				SummaryTitle = "Sync failed with " + ErrorCount.ToString() + " errors";
				SummaryIcon = ToolTipIcon.Error;
			}
			else
			{
				SummaryTitle = "Sync succeeded";
				SummaryIcon = ToolTipIcon.Info;
			}

			SummaryText = FilesSynced.ToString() + " files synced taking " + TotalDuration.TotalSeconds.ToString( "F0" ) + " seconds.";
		}

		private void CloseLog()
		{
            if( Log != null )
            {
				WriteToLog( "Total errors: " + ErrorCount.ToString() );
				WriteToLog( "Total warnings: " + WarningCount.ToString() );
				WriteToLog( "Total messages: " + MessageCount.ToString() );
				WriteToLog( "Total files synced: " + FilesSynced.ToString() );
				WriteToLog( "Total duration: " + TotalDuration.TotalSeconds.ToString( "F0" ) + " seconds" );
				WriteToLog( "End of sync" );
				
				Log.Close();
				Log = null;
            }
        }

		private void WriteToLog( string Line )
		{
			Log.WriteLine( DateTime.Now.ToString( "HH:mm:ss" ) + ": " + Line );
		}

		private void WriteToLog( P4RecordSet Results )
		{
			WriteToLog( Results.Errors.Length.ToString() + " errors found" );
			foreach( string Error in Results.Errors )
			{
				string ErrorMessage = Error.Replace( "\r", "" );
				ErrorMessage = ErrorMessage.Replace( "\n", " " );
				WriteToLog( "ERROR: " + ErrorMessage );
			}

			WriteToLog( Results.Warnings.Length.ToString() + " warnings found" );
			foreach( string Warning in Results.Warnings )
			{
				string WarningMessage = Warning.Replace( "\r", "" );
				WarningMessage = WarningMessage.Replace( "\n", " " );
				WriteToLog( " ... " + WarningMessage );
			}

			WriteToLog( Results.Messages.Length.ToString() + " messages found" );
			foreach( string Message in Results.Messages )
			{
				WriteToLog( " ... " + Message.Replace( Environment.NewLine, "" ) );
			}

			WriteToLog( Results.Records.Length.ToString() + " records returned" );
			foreach( P4Record Record in Results )
			{
				WriteToLog( " ... " + Record["depotFile"] + "#" + Record["rev"] + " " + Record["action"] );
			}

			WriteToLog( "" );
		}

		private void CollateResults( P4RecordSet Results, TimeSpan Duration )
		{
			ErrorCount += Results.Errors.Length;
			WarningCount += Results.Warnings.Length;
			MessageCount += Results.Messages.Length;
			FilesSynced += Results.Records.Length;

			TotalDuration += Duration;
		}

		/** Dump the info about the revision to the log file */
		private void DumpRevisionInfo( BranchSpec Branch, string Revision )
		{
			int SpecificChangelist = -1;
			if( Int32.TryParse( Revision.TrimStart( "@".ToCharArray() ), out SpecificChangelist ) )
			{
				WriteToLog( " ... syncing to changelist " + SpecificChangelist.ToString() );
			}
			else if( Revision.ToLower() == "#head" )
			{
				P4RecordSet Results = IP4Net.Run( "changes", "-s", "submitted", "-m1", "//depot/" + Branch.DepotName + "/..." );
				if( Results != null && Results.Records.Length > 0 )
				{
					string ChangeListString = Results.Records[0].Fields["change"];
					WriteToLog( " ... syncing to #head (changelist " + ChangeListString + ")" );
				}
			}
			else
			{
				P4RecordSet Results = IP4Net.Run( "label", "-o", Revision.TrimStart( "@".ToCharArray() ) );
				if( Results != null && Results.Records.Length > 0 )
				{
					WriteToLog( " ... syncing to label " + Revision );
					WriteToLog( "" );

					string[] DescriptionLines = Results.Records[0].Fields["Description"].Split( "\n\r".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
					foreach( string Line in DescriptionLines )
					{
						WriteToLog( Line );
					}
				}
			}

			WriteToLog( "" );
		}

		/** Check to see if any writable files need to be clobbered */
		private void CheckClobber( P4RecordSet Results, string Revision )
		{
			if( Parent.Options.AutoClobber )
			{
				foreach( string Error in Results.Errors )
				{
					if( Error.StartsWith( "Can't clobber writable file " ) )
					{
						string FileName = Error.Substring( "Can't clobber writable file ".Length );
						foreach( P4Record Record in Results )
						{
							if( FileName == Record["clientFile"] )
							{
								FileName = Record["depotFile"];
								break;
							}
						}

						P4RecordSet ForceSyncResults = IP4Net.Run( "sync", "-f", FileName + Revision );
						WriteToLog( "Command: p4 sync -f " + FileName + Revision );
						WriteToLog( ForceSyncResults );

						if( ForceSyncResults.Errors.Length == 0 )
						{
							ErrorCount--;
						}
					}
				}
			}
		}

		/** Apply the desired resolve paradigm */
		private void Resolve( BranchSpec Branch )
		{
			string ResolveParameter = "";
			switch( Parent.Options.ResolveType )
			{
			case EResolveType.None:
				break;

			case EResolveType.Automatic:
				ResolveParameter = "-am";
				break;

			case EResolveType.SafeAutomatic:
				ResolveParameter = "-as";
				break;

			case EResolveType.SafeAutoWithConflicts:
				ResolveParameter = "-af";
				break;
			}

			if( ResolveParameter.Length > 0 )
			{
				DateTime StartTime = DateTime.UtcNow;
				P4RecordSet Results = IP4Net.Run( "resolve", ResolveParameter, "//depot/" + Branch.DepotName + "/..." );
				TimeSpan Duration = DateTime.UtcNow - StartTime;
				CollateResults( Results, Duration );

				WriteToLog( "Command: p4 resolve " + ResolveParameter + " //depot/" + Branch.DepotName + "/... (" + Duration.TotalSeconds.ToString( "F0" ) + " seconds)" );
				WriteToLog( Results );
			}
		}

		/** Sync an entire branch to a single specified revision */
		public void SyncRevision( BranchSpec Branch, string Revision )
		{
			try
			{
				SummaryIcon = ToolTipIcon.Info;
				OpenLog( Branch );

				DateTime StartTime = DateTime.UtcNow;

				IP4Net.Port = Branch.Server;
				IP4Net.Client = Branch.ClientSpec;
				IP4Net.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;

				DumpRevisionInfo( Branch, Revision );

				string Parameters = "//depot/" + Branch.DepotName + "/..." + Revision;
				P4RecordSet Results = IP4Net.Run( "sync", Parameters );

				TimeSpan Duration = DateTime.UtcNow - StartTime;
				CollateResults( Results, Duration );

				WriteToLog( "Command: p4 sync " + Parameters + " (" + Duration.TotalSeconds.ToString( "F0" ) + " seconds)" );
				WriteToLog( Results );

				CheckClobber( Results, Revision );
				Resolve( Branch );

				IP4Net.Disconnect();
				CloseLog();

				AnalyseResults();
			}
			catch( Exception Ex )
			{
				WriteToLog( "ERROR: Exception During Sync!" );
				WriteToLog( Ex.Message );
				IP4Net.Disconnect();
				CloseLog();

				SummaryTitle = "Exception During Sync!";
				SummaryText = Ex.Message;
				SummaryIcon = ToolTipIcon.Error;
			}
		}

		/** Sync a mixture of files to various revisions e.g. and ArtistSync */
		public void SyncRevision( BranchSpec Branch, string Revision, List<string> SyncCommands )
		{
			try
			{
				SummaryIcon = ToolTipIcon.Info;
				OpenLog( Branch );

				IP4Net.Port = Branch.Server;
				IP4Net.Client = Branch.ClientSpec;
				IP4Net.ExceptionLevel = P4ExceptionLevels.NoExceptionOnErrors;

				DumpRevisionInfo( Branch, Revision );

				foreach( string SyncCommand in SyncCommands )
				{
					DateTime StartTime = DateTime.UtcNow;

					string Parameters = "//depot/" + Branch.DepotName + SyncCommand.Replace( "%LABEL_TO_SYNC_TO%", Revision );
					P4RecordSet Results = IP4Net.Run( "sync", Parameters );

					TimeSpan Duration = DateTime.UtcNow - StartTime;
					CollateResults( Results, Duration );

					WriteToLog( "Command: p4 sync " + Parameters + " (" + Duration.TotalSeconds.ToString( "F0" ) + " seconds)" );
					WriteToLog( Results );

					CheckClobber( Results, Revision );
				}

				Resolve( Branch );

				IP4Net.Disconnect();
				CloseLog();

				AnalyseResults();
			}
			catch( Exception Ex )
			{
				WriteToLog( "ERROR: Exception During Sync!" );
				WriteToLog( Ex.Message );
				IP4Net.Disconnect();
				CloseLog();

				SummaryTitle = "Exception During Sync!";
				SummaryText = Ex.Message;
				SummaryIcon = ToolTipIcon.Error;
			}
		}
	}
}
