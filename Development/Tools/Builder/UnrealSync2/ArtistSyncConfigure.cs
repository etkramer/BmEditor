/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace UnrealSync2
{
	public partial class ArtistSyncConfigure : Form
	{
		private UnrealSync2 Main = null;

		public ArtistSyncConfigure( UnrealSync2 InMain )
		{
			Main = InMain;

			InitializeComponent();

			PopulateBranchDataGrid();
		}

		public void ArtistSyncConfigureOKButtonClicked( object sender, EventArgs e )
		{
			foreach( DataGridViewRow Row in ArtistSyncDataGridView.Rows )
			{
				BranchSpec Branch = ( BranchSpec )Row.Tag;

				string GameName = ( string )Row.Cells[2].Value;
				string SyncTimeString = ( string )Row.Cells[3].Value;
				DateTime SyncTime = Main.GetSyncTime( SyncTimeString );

				Main.SetPromotableGameSyncTime( Branch, GameName, SyncTime );
			}

			Close();
		}

		private void PopulateBranchDataGrid()
		{
			DataGridViewTextBoxCell TextBoxCell = null;
			DataGridViewComboBoxCell ComboBoxCell = null;
			
			List<string> UniqueClientSpecs = Main.GetUniqueClientSpecs( true );
			List<DataGridViewCellStyle> CellStyles = Main.GetGridStyles( UniqueClientSpecs.Count );

			// Create the datagrid view
			foreach( BranchSpec Branch in Main.BranchSpecs )
			{
				foreach( UnrealSync2.PromotableGame Game in Branch.PromotableGames )
				{
					DataGridViewRow Row = new DataGridViewRow();

					Row.Tag = ( object )Branch;
					Row.DefaultCellStyle = CellStyles[UniqueClientSpecs.IndexOf( Branch.ClientSpec )];

					// TextBox for the branch root
					TextBoxCell = new DataGridViewTextBoxCell();
					TextBoxCell.Value = Branch.ClientSpec + " (" + Branch.Root + ")";
					Row.Cells.Add( TextBoxCell );

					// TextBox for the branch name
					TextBoxCell = new DataGridViewTextBoxCell();
					TextBoxCell.Value = Branch.Name;
					Row.Cells.Add( TextBoxCell );

					// TextBox for the branch name
					TextBoxCell = new DataGridViewTextBoxCell();
					TextBoxCell.Value = Game.GameName;
					Row.Cells.Add( TextBoxCell );

					// ComboBox for the time to sync
					ComboBoxCell = new DataGridViewComboBoxCell();
					ComboBoxCell.Items.AddRange( SyncTime.Items );
					if( Game.SyncTime == DateTime.MaxValue )
					{
						ComboBoxCell.Value = "Never";
					}
					else
					{
						ComboBoxCell.Value = Game.SyncTime.ToString( "h:mm tt" );
					}

					Row.Cells.Add( ComboBoxCell );

					ArtistSyncDataGridView.Rows.Add( Row );
				}
			}
		}
	}
}
