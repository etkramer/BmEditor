/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Data.SqlClient;
using System.Linq;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;

public partial class CISLatency : BasePage
{
	private class CISLatencySample
	{
		public string Name = "";
		public DateTime CreationTime = DateTime.MinValue;
		public DateTime StartTime = DateTime.MinValue;
		public DateTime EndTime = DateTime.MinValue;
	}

	private List<CISLatencySample> GetRawData( SqlConnection Connection )
	{
		List<CISLatencySample> Samples = new List<CISLatencySample>();

		string Query = "SELECT Jobs.Name, Jobs.SpawnTime, BuildLog.BuildStarted, BuildLog.BuildEnded FROM Jobs " +
						"INNER JOIN BuildLog ON BuildLog.ID = Jobs.BuildLogID " +
						"WHERE LEFT( Jobs.Name, 16 ) = 'CIS Code Builder' AND Complete = 1 AND DATEDIFF( day, BuildLog.BuildStarted, GETDATE() ) < 8 " +
						"ORDER BY SpawnTime ASC";

		using( SqlCommand Command = new SqlCommand( Query, Connection ) )
		{
			SqlDataReader Reader = Command.ExecuteReader();
			if( Reader.HasRows )
			{
				while( Reader.Read() )
				{
					CISLatencySample Sample = new CISLatencySample();
					Sample.Name = Reader.GetString( 0 );
					Sample.CreationTime = new DateTime( Reader.GetInt64( 1 ) );
					Sample.StartTime = Reader.GetDateTime( 2 );
					Sample.EndTime = Reader.GetDateTime( 3 );
					Samples.Add( Sample );
				}
			}
			Reader.Close();
		}

		return( Samples );
	}

	private void PopulateChart( List<CISLatencySample> Samples )
	{
		double WaitTime = 0.0f;
		double Duration = 0.0f;
		double TotalWaitTime = 0.0f;
		double TotalDuration = 0.0f;
		int SampleCount = 0;
		DateTime StartTime = Samples[0].CreationTime;
		DateTime EndTime = Samples[0].CreationTime.AddHours( 1 );

		foreach( CISLatencySample Sample in Samples )
		{
			if( Sample.CreationTime < EndTime )
			{
				WaitTime = Math.Max( 0.0, ( Sample.StartTime - Sample.CreationTime ).TotalMinutes );
				Duration = Math.Max( 0.0, ( Sample.EndTime - Sample.StartTime ).TotalMinutes );
				if( WaitTime < 12.0 * 60.0 && Duration < 12.0 * 60.0 )
				{
					TotalWaitTime += WaitTime;
					TotalDuration += Duration;
					SampleCount++;
				}
			}
			else
			{
				WaitTime = 0.0f;
				Duration = 0.0f;
				if( SampleCount > 0 )
				{
					WaitTime = TotalWaitTime / SampleCount;
					Duration = TotalDuration / SampleCount;
				}

				CISLatencyChart.Series["WaitTime"].Points.AddXY( StartTime, WaitTime );
				CISLatencyChart.Series["Duration"].Points.AddXY( StartTime, Duration );

				TotalWaitTime = ( Sample.StartTime - Sample.CreationTime ).TotalMinutes;
				TotalDuration = ( Sample.EndTime - Sample.StartTime ).TotalMinutes;
				SampleCount = 1;

				StartTime = EndTime;
				EndTime = EndTime.AddHours( 1 );
			}
		}
	}

	protected void Page_Load( object sender, EventArgs e )
	{
		List<CISLatencySample> Samples = null;
		using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
		{
			Connection.Open();
			Samples = GetRawData( Connection );
			Connection.Close();
		}

		if( Samples != null && Samples.Count > 0 )
		{
			PopulateChart( Samples );
		}
	}
}
