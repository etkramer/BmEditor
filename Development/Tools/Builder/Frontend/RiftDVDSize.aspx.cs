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

public partial class RiftDVDSize : BasePage
{
	private void FillSeries( SqlConnection Connection, string Item, int CounterID )
	{
		using( SqlCommand Command = new SqlCommand( "SELECT DateTimeStamp, IntValue / ( 1024 * 1024 ) AS " + Item + " FROM PerformanceData " +
													"WHERE ( CounterID = " + CounterID.ToString() + " ) AND ( DATEDIFF( day, DateTimeStamp, GETDATE() ) < 90 ) " +
													"ORDER BY DateTimeStamp DESC", Connection ) )
		{
			SqlDataReader Reader = Command.ExecuteReader();
			DataTable Table = new DataTable();

			Table.Load( Reader );
			RiftDVDSizeChart.Series[Item].Points.DataBindXY( Table.Rows, "DateTimeStamp", Table.Rows, Item );

			Reader.Close();
		}
	}
	
	protected void Page_Load( object sender, EventArgs e )
	{
		using( SqlConnection Connection = new SqlConnection( ConfigurationManager.ConnectionStrings["BuilderConnectionString"].ConnectionString ) )
		{
			Connection.Open();
			FillSeries( Connection, "Xbox360DVDCapacity", 969 );
			FillSeries( Connection, "GearDVDSize_WWSKU", 1204 );
			FillSeries( Connection, "GearDVDSize_INT", 1193 );
			FillSeries( Connection, "GearDVDSize_INT_FRA", 1198 );
			FillSeries( Connection, "GearDVDSize_INT_ITA", 1199 );
			FillSeries( Connection, "GearDVDSize_INT_DEU", 1200 );
			FillSeries( Connection, "GearDVDSize_INT_ESN", 1201 );
			FillSeries( Connection, "GearDVDSize_INT_ESM", 1202 );
			FillSeries( Connection, "GearDVDSize_INT_JPN", 1203 );
			Connection.Close();
		}
	}
}
