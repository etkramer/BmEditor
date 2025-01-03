/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealBuildTool
{
	class UE3BuildBmGame : UE3BuildGame
	{
        /** Returns the singular name of the game being built ("ExampleGame", "UDKGame", etc) */
		public string GetGameName()
		{
			return "BmGame";
		}
		
		/** Returns a subplatform (e.g. dll) to disambiguate object files */
		public string GetSubPlatform()
		{
			return ( "" );
		}

		/** Get the desired OnlineSubsystem. */
		public string GetDesiredOnlineSubsystem( CPPEnvironment CPPEnv, UnrealTargetPlatform Platform )
		{
			string ForcedOSS = UE3BuildTarget.ForceOnlineSubsystem( Platform );
			if( ForcedOSS != null )
			{
				return ( ForcedOSS );
			}

			return ( "PC" );
		}

		/** Returns true if the game wants to have PC ES2 simulator (ie ES2 Dynamic RHI) enabled */
		public bool ShouldCompileES2()
		{
			return true;
		}

        /** Allows the game add any global environment settings before building */
        public void GetGameSpecificGlobalEnvironment(CPPEnvironment GlobalEnvironment)
        {

        }

        /** Returns the xex.xml file for the given game */
		public FileItem GetXEXConfigFile()
		{
			return FileItem.GetExistingItemByPath("BmGame/Live/xex.xml");
		}

        public void SetUpGameEnvironment(CPPEnvironment GameCPPEnvironment, LinkEnvironment FinalLinkEnvironment, List<UE3ProjectDesc> GameProjects)
        {
            GameCPPEnvironment.IncludePaths.Add("BmGame/Inc");
            GameProjects.Add(new UE3ProjectDesc("BmGame/BmGame.vcproj"));

            if (UE3BuildConfiguration.bBuildEditor &&
                (GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win32 || GameCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win64))
            {
                GameProjects.Add(new UE3ProjectDesc("BmEditor/BmEditor.vcproj"));
                GameCPPEnvironment.IncludePaths.Add("BmEditor/Inc");
            }

            GameCPPEnvironment.Definitions.Add("GAMENAME=BMGAME");
            GameCPPEnvironment.Definitions.Add("IS_BMGAME=1");
        }
	}
}
