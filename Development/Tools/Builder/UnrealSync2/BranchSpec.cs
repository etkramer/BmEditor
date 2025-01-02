/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Windows.Forms;

namespace UnrealSync2
{
	public class BranchSpec
	{
		// Persistent info
		public string Server = "";
		public string ClientSpec = "";
		public string Name = "";
		public bool bDisplayInMenu = true;
		public ESyncType SyncType = ESyncType.Head;
		public DateTime SyncTime = DateTime.MaxValue;
		public TimeSpan SyncRandomisation = new TimeSpan( 0, new Random().Next( 30 ), 0 );
		public List<UnrealSync2.PromotableGame> PromotableGames;

		// Info used at run time
		public bool bCISAvailable = false;
		public bool bIsMain = false;
		public bool bImmediateSync = false;
		public ESyncType ImmediateSyncType = ESyncType.Head;
		public string GameName = "";
		public string Root = "";
		public string DepotName = "";
		public string HeadChangelist = "";
		public string LastGoodCIS = "";
		public string LatestBuild = "";
		public string LatestApprovedBuild = "";
		public string LatestQABuild = "";
		public ToolStripMenuItem MenuItem = null;

		public BranchSpec( string InServer, string InClientSpec, string InName, string InDepotName, string InRoot )
		{
			Server = InServer;
			ClientSpec = InClientSpec;
			Name = InName;
			DepotName = InDepotName;
			Root = InRoot;
			PromotableGames = new List<UnrealSync2.PromotableGame>();

			bIsMain = ( Name.ToLower() == "unrealengine3" );
		}

		public override string ToString()
		{
			return ( Server.Replace( ':', '-' ) + "_" + ClientSpec + "_" + Name );
		}
	}
}