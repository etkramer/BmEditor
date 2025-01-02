/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Data;
using System.Data.SqlClient;

namespace UnrealSync2
{
	public class DataBase
	{
		public DataBase()
        {
        }

		private int GetInt( SqlConnection Connection, string Query )
		{
			int Result = -1;

			try
			{
				using( SqlCommand Command = new SqlCommand( Query, Connection ) )
				{
					SqlDataReader DataReader = Command.ExecuteReader();
					if( DataReader.Read() )
					{
						Result = DataReader.GetInt32( 0 );
					}

					DataReader.Close();
				}
			}
			catch
			{
			}

			return ( Result );
		}

		private string GetString( SqlConnection Connection, string Query )
		{
			string Result = "";

			try
			{
				using( SqlCommand Command = new SqlCommand( Query, Connection ) )
				{
					SqlDataReader DataReader = Command.ExecuteReader();
					if( DataReader.Read() )
					{
						Result = DataReader.GetString( 0 );
					}

					DataReader.Close();
				}
			}
			catch
			{
			}

			return ( Result );
		}

		private List<string> GetStringCollection( SqlConnection Connection, string Query )
		{
			List<string> Result = new List<string>();

			try
			{
				using( SqlCommand Command = new SqlCommand( Query, Connection ) )
				{
					SqlDataReader DataReader = Command.ExecuteReader();
					while( DataReader.Read() )
					{
						Result.Add( DataReader.GetString( 0 ) );
					}

					DataReader.Close();
				}
			}
			catch
			{
			}

			return ( Result );
		}

		public List<string> GetPerforceServers()
		{
			List<string> PerforceServers = new List<string>();
			try
			{
				using( SqlConnection Connection = new SqlConnection( Properties.Settings.Default.ConnectionString ) )
				{
					Connection.Open();

					PerforceServers = GetStringCollection( Connection, "SELECT DISTINCT( Server ) FROM BranchConfig" );

					Connection.Close();
				}
			}
			catch
			{
			}

			return ( PerforceServers );
		}

		public string GetBuild( string BuildType, string BranchName )
		{
			string Label = "";
			try
			{
				using( SqlConnection Connection = new SqlConnection( Properties.Settings.Default.ConnectionString ) )
				{
					Connection.Open();

					Label = GetString( Connection, "SELECT Value FROM Variables WHERE ( Variable = '" + BuildType + "' AND Branch = '" + BranchName + "' )" );

					Connection.Close();
				}
			}
			catch
			{
			}

			return( Label );
		}

		public int GetCISVariable( string CISVariable, string BranchName )
		{
			int ChangeList = -1;
			try
			{
				using( SqlConnection Connection = new SqlConnection( Properties.Settings.Default.ConnectionString ) )
				{
					Connection.Open();

					ChangeList = GetInt( Connection, "SELECT " + CISVariable + " FROM BranchConfig WHERE ( Branch = '" + BranchName + "' )" );

					Connection.Close();
				}
			}
			catch
			{
			}

			return ( ChangeList );
		}

		public void PopulateBranchSpecs( UnrealSync2 Main )
		{
			try
			{
				using( SqlConnection Connection = new SqlConnection( Properties.Settings.Default.ConnectionString ) )
				{
					Connection.Open();

					foreach( BranchSpec Branch in Main.BranchSpecs )
					{
						int LatestGoodCISChangelist = GetInt( Connection, "SELECT LastGoodOverall FROM BranchConfig WHERE ( Branch = '" + Branch.Name + "' )" );
						int LatestAttemptedCISChangelist = GetInt( Connection, "SELECT HeadChangelist FROM BranchConfig WHERE ( Branch = '" + Branch.Name + "' )" );
						Branch.LastGoodCIS = "";
						Branch.bCISAvailable = ( LatestAttemptedCISChangelist > 0 ) && ( LatestGoodCISChangelist > 0 );
						if( Branch.bCISAvailable )
						{
							Branch.LastGoodCIS = LatestGoodCISChangelist.ToString();
							Branch.HeadChangelist = LatestAttemptedCISChangelist.ToString();
						}

						Branch.LatestBuild = GetBuild( "LatestBuild", Branch.Name );
						Branch.LatestApprovedBuild = GetBuild( "LatestApprovedBuild", Branch.Name );
						Branch.LatestQABuild = GetBuild( "LatestApprovedQABuild", Branch.Name );
					}

					Connection.Close();
				}
			}
			catch
			{
			}
		}
	}
}