namespace UnrealSync2
{
	partial class ArtistSyncConfigure
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose( bool disposing )
		{
			if( disposing && ( components != null ) )
			{
				components.Dispose();
			}
			base.Dispose( disposing );
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager( typeof( ArtistSyncConfigure ) );
			this.ButtonAccept = new System.Windows.Forms.Button();
			this.ArtistSyncDataGridView = new System.Windows.Forms.DataGridView();
			this.PerforceClient = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.BranchName = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.GameName = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.SyncTime = new System.Windows.Forms.DataGridViewComboBoxColumn();
			( ( System.ComponentModel.ISupportInitialize )( this.ArtistSyncDataGridView ) ).BeginInit();
			this.SuspendLayout();
			// 
			// ButtonAccept
			// 
			this.ButtonAccept.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.ButtonAccept.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.ButtonAccept.Location = new System.Drawing.Point( 731, 170 );
			this.ButtonAccept.Name = "ButtonAccept";
			this.ButtonAccept.Size = new System.Drawing.Size( 75, 23 );
			this.ButtonAccept.TabIndex = 7;
			this.ButtonAccept.Text = "OK";
			this.ButtonAccept.UseVisualStyleBackColor = true;
			this.ButtonAccept.Click += new System.EventHandler( this.ArtistSyncConfigureOKButtonClicked );
			// 
			// ArtistSyncDataGridView
			// 
			this.ArtistSyncDataGridView.AllowUserToAddRows = false;
			this.ArtistSyncDataGridView.AllowUserToDeleteRows = false;
			this.ArtistSyncDataGridView.AllowUserToOrderColumns = true;
			this.ArtistSyncDataGridView.AllowUserToResizeRows = false;
			this.ArtistSyncDataGridView.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.ArtistSyncDataGridView.AutoSizeColumnsMode = System.Windows.Forms.DataGridViewAutoSizeColumnsMode.Fill;
			this.ArtistSyncDataGridView.BackgroundColor = System.Drawing.SystemColors.Control;
			this.ArtistSyncDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
			this.ArtistSyncDataGridView.Columns.AddRange( new System.Windows.Forms.DataGridViewColumn[] {
            this.PerforceClient,
            this.BranchName,
            this.GameName,
            this.SyncTime} );
			this.ArtistSyncDataGridView.GridColor = System.Drawing.SystemColors.Control;
			this.ArtistSyncDataGridView.Location = new System.Drawing.Point( 1, 1 );
			this.ArtistSyncDataGridView.Name = "ArtistSyncDataGridView";
			this.ArtistSyncDataGridView.Size = new System.Drawing.Size( 817, 154 );
			this.ArtistSyncDataGridView.TabIndex = 1;
			// 
			// PerforceClient
			// 
			this.PerforceClient.HeaderText = "Perforce Client";
			this.PerforceClient.Name = "PerforceClient";
			this.PerforceClient.ReadOnly = true;
			// 
			// BranchName
			// 
			this.BranchName.HeaderText = "Branch Name";
			this.BranchName.Name = "BranchName";
			this.BranchName.ReadOnly = true;
			// 
			// GameName
			// 
			this.GameName.FillWeight = 50F;
			this.GameName.HeaderText = "GameName";
			this.GameName.Name = "GameName";
			this.GameName.ReadOnly = true;
			// 
			// SyncTime
			// 
			this.SyncTime.FillWeight = 50F;
			this.SyncTime.HeaderText = "Sync Time";
			this.SyncTime.Items.AddRange( new object[] {
            "Never",
            "3:00 AM",
            "3:30 AM",
            "4:00 AM",
            "4:30 AM",
            "5:00 AM",
            "5:30 AM",
            "6:00 AM",
            "6:30 AM",
            "7:00 AM",
            "7:30 AM",
            "8:00 AM",
            "8:30 AM",
            "9:00 AM",
            "9:30 AM",
            "10:00 AM",
            "10:30 AM",
            "11:00 AM",
            "11:30 AM",
            "12:00 PM"} );
			this.SyncTime.Name = "SyncTime";
			// 
			// ArtistSyncConfigure
			// 
			this.AcceptButton = this.ButtonAccept;
			this.AutoScaleDimensions = new System.Drawing.SizeF( 6F, 13F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size( 818, 205 );
			this.Controls.Add( this.ArtistSyncDataGridView );
			this.Controls.Add( this.ButtonAccept );
			this.Icon = ( ( System.Drawing.Icon )( resources.GetObject( "$this.Icon" ) ) );
			this.Name = "ArtistSyncConfigure";
			this.Text = "Configure Artist Syncs";
			( ( System.ComponentModel.ISupportInitialize )( this.ArtistSyncDataGridView ) ).EndInit();
			this.ResumeLayout( false );

		}

		#endregion

		private System.Windows.Forms.Button ButtonAccept;
		private System.Windows.Forms.DataGridView ArtistSyncDataGridView;
		private System.Windows.Forms.DataGridViewTextBoxColumn PerforceClient;
		private System.Windows.Forms.DataGridViewTextBoxColumn BranchName;
		private System.Windows.Forms.DataGridViewTextBoxColumn GameName;
		private System.Windows.Forms.DataGridViewComboBoxColumn SyncTime;
	}
}