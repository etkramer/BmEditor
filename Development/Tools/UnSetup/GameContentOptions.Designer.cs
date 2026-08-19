/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
// BM
namespace UnSetup
{
	partial class GameContentOptions
	{
		private System.ComponentModel.IContainer components = null;

		protected override void Dispose( bool disposing )
		{
			if( disposing && ( components != null ) )
			{
				components.Dispose();
			}
			base.Dispose( disposing );
		}

		#region Windows Form Designer generated code

		private void InitializeComponent()
		{
			this.GameLocationTextbox = new System.Windows.Forms.TextBox();
			this.ChooseGameLocationButton = new System.Windows.Forms.Button();
			this.GameContentTitleLabel = new System.Windows.Forms.Label();
			this.ChooseGameLocationBrowser = new System.Windows.Forms.FolderBrowserDialog();
			this.GameContentCancelButton = new System.Windows.Forms.Button();
			this.GameContentOKButton = new System.Windows.Forms.Button();
			this.GameLocationGroupBox = new System.Windows.Forms.GroupBox();
			this.GameContentHeaderLine = new System.Windows.Forms.Label();
			this.GameContentFooterLine = new System.Windows.Forms.Label();
			this.GameContentFooterLineLabel = new System.Windows.Forms.Label();
			this.GameContentDescriptionLabel = new System.Windows.Forms.Label();
			this.InvalidGameLocationLabel = new System.Windows.Forms.Label();
			this.GameLocationGroupBox.SuspendLayout();
			this.SuspendLayout();
			//
			// GameLocationTextbox
			//
			this.GameLocationTextbox.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.GameLocationTextbox.Location = new System.Drawing.Point( 11, 23 );
			this.GameLocationTextbox.Margin = new System.Windows.Forms.Padding( 8, 4, 4, 4 );
			this.GameLocationTextbox.Name = "GameLocationTextbox";
			this.GameLocationTextbox.Size = new System.Drawing.Size( 654, 23 );
			this.GameLocationTextbox.TabIndex = 0;
			this.GameLocationTextbox.TextChanged += new System.EventHandler( this.GameLocationTextChanged );
			//
			// ChooseGameLocationButton
			//
			this.ChooseGameLocationButton.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.ChooseGameLocationButton.Location = new System.Drawing.Point( 673, 23 );
			this.ChooseGameLocationButton.Margin = new System.Windows.Forms.Padding( 4, 4, 8, 4 );
			this.ChooseGameLocationButton.Name = "ChooseGameLocationButton";
			this.ChooseGameLocationButton.Size = new System.Drawing.Size( 80, 23 );
			this.ChooseGameLocationButton.TabIndex = 1;
			this.ChooseGameLocationButton.Text = "Browse...";
			this.ChooseGameLocationButton.UseVisualStyleBackColor = true;
			this.ChooseGameLocationButton.MouseClick += new System.Windows.Forms.MouseEventHandler( this.ChooseGameLocationClick );
			//
			// GameContentTitleLabel
			//
			this.GameContentTitleLabel.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.GameContentTitleLabel.BackColor = System.Drawing.Color.White;
			this.GameContentTitleLabel.Font = new System.Drawing.Font( "Tahoma", 12F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.GameContentTitleLabel.Image = global::UnSetup.Properties.Resources.BannerImage;
			this.GameContentTitleLabel.Location = new System.Drawing.Point( -3, 0 );
			this.GameContentTitleLabel.Name = "GameContentTitleLabel";
			this.GameContentTitleLabel.Size = new System.Drawing.Size( 800, 68 );
			this.GameContentTitleLabel.TabIndex = 9;
			this.GameContentTitleLabel.Text = "Title";
			this.GameContentTitleLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			//
			// ChooseGameLocationBrowser
			//
			this.ChooseGameLocationBrowser.RootFolder = System.Environment.SpecialFolder.MyComputer;
			//
			// GameContentCancelButton
			//
			this.GameContentCancelButton.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.GameContentCancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.GameContentCancelButton.Location = new System.Drawing.Point( 682, 285 );
			this.GameContentCancelButton.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.GameContentCancelButton.Name = "GameContentCancelButton";
			this.GameContentCancelButton.Size = new System.Drawing.Size( 100, 32 );
			this.GameContentCancelButton.TabIndex = 4;
			this.GameContentCancelButton.Text = "Cancel";
			this.GameContentCancelButton.UseVisualStyleBackColor = true;
			this.GameContentCancelButton.Click += new System.EventHandler( this.CancelButtonClick );
			//
			// GameContentOKButton
			//
			this.GameContentOKButton.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.GameContentOKButton.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.GameContentOKButton.Location = new System.Drawing.Point( 576, 285 );
			this.GameContentOKButton.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.GameContentOKButton.Name = "GameContentOKButton";
			this.GameContentOKButton.Size = new System.Drawing.Size( 100, 32 );
			this.GameContentOKButton.TabIndex = 3;
			this.GameContentOKButton.Text = "OK";
			this.GameContentOKButton.UseVisualStyleBackColor = true;
			this.GameContentOKButton.Click += new System.EventHandler( this.OKButtonClick );
			//
			// GameLocationGroupBox
			//
			this.GameLocationGroupBox.Controls.Add( this.GameLocationTextbox );
			this.GameLocationGroupBox.Controls.Add( this.ChooseGameLocationButton );
			this.GameLocationGroupBox.Location = new System.Drawing.Point( 18, 162 );
			this.GameLocationGroupBox.Name = "GameLocationGroupBox";
			this.GameLocationGroupBox.Size = new System.Drawing.Size( 764, 61 );
			this.GameLocationGroupBox.TabIndex = 0;
			this.GameLocationGroupBox.TabStop = false;
			this.GameLocationGroupBox.Text = "Game Location";
			//
			// GameContentHeaderLine
			//
			this.GameContentHeaderLine.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.GameContentHeaderLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.GameContentHeaderLine.Location = new System.Drawing.Point( -3, 68 );
			this.GameContentHeaderLine.Name = "GameContentHeaderLine";
			this.GameContentHeaderLine.Size = new System.Drawing.Size( 800, 2 );
			this.GameContentHeaderLine.TabIndex = 23;
			//
			// GameContentFooterLine
			//
			this.GameContentFooterLine.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.GameContentFooterLine.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
			this.GameContentFooterLine.Location = new System.Drawing.Point( -3, 276 );
			this.GameContentFooterLine.Margin = new System.Windows.Forms.Padding( 3 );
			this.GameContentFooterLine.Name = "GameContentFooterLine";
			this.GameContentFooterLine.Size = new System.Drawing.Size( 800, 2 );
			this.GameContentFooterLine.TabIndex = 24;
			//
			// GameContentFooterLineLabel
			//
			this.GameContentFooterLineLabel.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.GameContentFooterLineLabel.AutoSize = true;
			this.GameContentFooterLineLabel.Enabled = false;
			this.GameContentFooterLineLabel.FlatStyle = System.Windows.Forms.FlatStyle.System;
			this.GameContentFooterLineLabel.Location = new System.Drawing.Point( 12, 268 );
			this.GameContentFooterLineLabel.Name = "GameContentFooterLineLabel";
			this.GameContentFooterLineLabel.Size = new System.Drawing.Size( 87, 16 );
			this.GameContentFooterLineLabel.TabIndex = 25;
			this.GameContentFooterLineLabel.Text = " UDK-2009-09";
			//
			// GameContentDescriptionLabel
			//
			this.GameContentDescriptionLabel.Location = new System.Drawing.Point( 18, 87 );
			this.GameContentDescriptionLabel.Name = "GameContentDescriptionLabel";
			this.GameContentDescriptionLabel.Size = new System.Drawing.Size( 764, 66 );
			this.GameContentDescriptionLabel.TabIndex = 26;
			this.GameContentDescriptionLabel.Text = "Description";
			//
			// InvalidGameLocationLabel
			//
			this.InvalidGameLocationLabel.AutoSize = true;
			this.InvalidGameLocationLabel.ForeColor = System.Drawing.Color.Firebrick;
			this.InvalidGameLocationLabel.Image = global::UnSetup.Properties.Resources.red_arrow;
			this.InvalidGameLocationLabel.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
			this.InvalidGameLocationLabel.Location = new System.Drawing.Point( 26, 234 );
			this.InvalidGameLocationLabel.Name = "InvalidGameLocationLabel";
			this.InvalidGameLocationLabel.Size = new System.Drawing.Size( 141, 16 );
			this.InvalidGameLocationLabel.TabIndex = 27;
			this.InvalidGameLocationLabel.Text = "   Invalid game location";
			//
			// GameContentOptions
			//
			this.AcceptButton = this.GameContentOKButton;
			this.AutoScaleDimensions = new System.Drawing.SizeF( 7F, 16F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.GameContentCancelButton;
			this.ClientSize = new System.Drawing.Size( 794, 330 );
			this.Controls.Add( this.InvalidGameLocationLabel );
			this.Controls.Add( this.GameContentDescriptionLabel );
			this.Controls.Add( this.GameContentFooterLineLabel );
			this.Controls.Add( this.GameContentFooterLine );
			this.Controls.Add( this.GameContentHeaderLine );
			this.Controls.Add( this.GameContentOKButton );
			this.Controls.Add( this.GameContentCancelButton );
			this.Controls.Add( this.GameContentTitleLabel );
			this.Controls.Add( this.GameLocationGroupBox );
			this.Font = new System.Drawing.Font( "Tahoma", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = global::UnSetup.Properties.Resources.UDKIcon;
			this.Margin = new System.Windows.Forms.Padding( 3, 4, 3, 4 );
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "GameContentOptions";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "Title";
			this.Load += new System.EventHandler( this.OnLoad );
			this.GameLocationGroupBox.ResumeLayout( false );
			this.GameLocationGroupBox.PerformLayout();
			this.ResumeLayout( false );
			this.PerformLayout();
		}

		#endregion

		private System.Windows.Forms.TextBox GameLocationTextbox;
		private System.Windows.Forms.Button ChooseGameLocationButton;
		private System.Windows.Forms.Label GameContentTitleLabel;
		private System.Windows.Forms.FolderBrowserDialog ChooseGameLocationBrowser;
		private System.Windows.Forms.Button GameContentCancelButton;
		private System.Windows.Forms.Button GameContentOKButton;
		private System.Windows.Forms.GroupBox GameLocationGroupBox;
		private System.Windows.Forms.Label GameContentHeaderLine;
		private System.Windows.Forms.Label GameContentFooterLine;
		private System.Windows.Forms.Label GameContentFooterLineLabel;
		private System.Windows.Forms.Label GameContentDescriptionLabel;
		private System.Windows.Forms.Label InvalidGameLocationLabel;
	}
}
