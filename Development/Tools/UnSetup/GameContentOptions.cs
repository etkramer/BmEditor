/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
// BM: Asks for the game install that content is copied out of at install time.
using System;
using System.IO;
using System.Windows.Forms;

namespace UnSetup
{
	public partial class GameContentOptions : Form
	{
		public GameContentOptions()
		{
			InitializeComponent();

			Text = Program.Util.GetPhrase( "GCTitle" );
			GameContentTitleLabel.Text = Text;
			GameContentFooterLineLabel.Text = Program.Util.UnSetupVersionString;
			GameContentDescriptionLabel.Text = Program.Util.GetPhrase( "GCDescription" );
			GameLocationGroupBox.Text = Program.Util.GetPhrase( "GBGameLocation" );
			ChooseGameLocationButton.Text = Program.Util.GetPhrase( "GBInstallLocationBrowse" );
			GameContentOKButton.Text = Program.Util.GetPhrase( "GQOK" );
			GameContentCancelButton.Text = Program.Util.GetPhrase( "GQCancel" );
			InvalidGameLocationLabel.Text = Program.Util.GetPhrase( "GCInvalidLocation" );

			GameLocationTextbox.Text = Program.Util.FindGameFolder();
		}

		public string GetGameLocation()
		{
			return ( Program.Util.ResolveGameFolder( GameLocationTextbox.Text.Trim() ) );
		}

		private void GameLocationTextChanged( object sender, EventArgs e )
		{
			bool bValid = GetGameLocation().Length > 0;

			InvalidGameLocationLabel.Visible = !bValid;
			GameContentOKButton.Enabled = bValid;
		}

		private void ChooseGameLocationClick( object sender, MouseEventArgs e )
		{
			ChooseGameLocationBrowser.SelectedPath = GameLocationTextbox.Text;
			if( ChooseGameLocationBrowser.ShowDialog() == DialogResult.OK )
			{
				GameLocationTextbox.Text = ChooseGameLocationBrowser.SelectedPath;
			}
		}

		private void OKButtonClick( object sender, EventArgs e )
		{
			DialogResult = DialogResult.OK;
			Close();
		}

		private void CancelButtonClick( object sender, EventArgs e )
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void OnLoad( object sender, EventArgs e )
		{
			Utils.CenterFormToPrimaryMonitor( this );

			// Resolve whatever was auto detected
			GameLocationTextChanged( sender, e );
		}
	}
}
