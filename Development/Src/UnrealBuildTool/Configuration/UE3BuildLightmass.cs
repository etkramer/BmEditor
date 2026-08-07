/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;

namespace UnrealBuildTool
{
	// BM: standalone target for the Lightmass tool, which uses LightmassCore instead of UE3 Core
	// and so shares none of the normal game/editor environment.
	partial class UE3BuildTarget
	{
		const string LightmassDirectory = "../Tools/UnrealLightmass";

		List<FileItem> BuildLightmass()
		{
			if (Platform != UnrealTargetPlatform.Win32 && Platform != UnrealTargetPlatform.Win64)
			{
				throw new BuildException("Lightmass can only be built for Win32 or Win64.");
			}

			bool bWin64 = (Platform == UnrealTargetPlatform.Win64);
			GlobalCPPEnvironment.TargetPlatform = bWin64 ? CPPTargetPlatform.Win64 : CPPTargetPlatform.Win32;
			FinalLinkEnvironment.TargetPlatform = GlobalCPPEnvironment.TargetPlatform;

			bool bDebug = (Configuration == UnrealTargetConfiguration.Debug);
			GlobalCPPEnvironment.TargetConfiguration = bDebug ? CPPTargetConfiguration.Debug : CPPTargetConfiguration.Release;
			FinalLinkEnvironment.TargetConfiguration = GlobalCPPEnvironment.TargetConfiguration;

			GlobalCPPEnvironment.OutputDirectory = Path.Combine(
				"../Intermediate/UnrealLightmass",
				Configuration.ToString() + "-" + Platform.ToString());

			GlobalCPPEnvironment.Definitions.Add("WIN32=1");
			GlobalCPPEnvironment.Definitions.Add("_CONSOLE=1");
			GlobalCPPEnvironment.Definitions.Add("_UNICODE=1");
			GlobalCPPEnvironment.Definitions.Add("UNICODE=1");
			GlobalCPPEnvironment.Definitions.Add(bDebug ? "_DEBUG=1" : "NDEBUG=1");
			if (bWin64)
			{
				GlobalCPPEnvironment.Definitions.Add("_WIN64=1");
			}
			GlobalCPPEnvironment.Definitions.AddRange(AdditionalDefinitions);

			// C4121: the x64 Windows SDK's DbgHelp.h trips this, and it's warnings-as-error by default.
			GlobalCPPEnvironment.AdditionalArguments += " /wd4121";

			string DXSDKDirectory = Environment.GetEnvironmentVariable("DXSDK_DIR");
			if (String.IsNullOrEmpty(DXSDKDirectory))
			{
				throw new BuildException("DXSDK_DIR is not set; Lightmass needs the DirectX SDK to build.");
			}

			GlobalCPPEnvironment.IncludePaths.Add(LightmassDirectory + "/Public");
			GlobalCPPEnvironment.IncludePaths.Add(LightmassDirectory + "/Inc");
			GlobalCPPEnvironment.IncludePaths.Add(LightmassDirectory + "/LightmassCore/Inc");
			GlobalCPPEnvironment.IncludePaths.Add(LightmassDirectory + "/Lighting/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("../Tools");
			GlobalCPPEnvironment.IncludePaths.Add("UnrealSwarm/Inc");
			GlobalCPPEnvironment.SystemIncludePaths.Add(Path.Combine(DXSDKDirectory, "include"));

			GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add("System.dll");
			GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add("System.Data.dll");
			GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add("System.Drawing.dll");
			GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add("System.Xml.dll");
			GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add("System.Management.dll");
			GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add("System.Windows.Forms.dll");
			AddPrivateAssembly(Path.GetDirectoryName(OutputPath) + "\\..\\", "", "AgentInterface.dll");

			FinalLinkEnvironment.LibraryPaths.Add(Path.Combine(DXSDKDirectory, bWin64 ? "lib/x64" : "lib/x86"));
			FinalLinkEnvironment.LibraryPaths.Add("../External/zlib/Lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("d3d9.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("d3dx9.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("dbghelp.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("Psapi.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("delayimp.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("kernel32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("user32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("advapi32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("ole32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("shell32.lib");
			FinalLinkEnvironment.bIsConsoleApplication = true;
			FinalLinkEnvironment.DelayLoadDLLs.Add("d3d9.dll");
			FinalLinkEnvironment.DelayLoadDLLs.Add("d3dx9_43.dll");
			FinalLinkEnvironment.DelayLoadDLLs.Add("dbghelp.dll");
			FinalLinkEnvironment.bUseUnrealEngine3Def = false;

			// Several Lightmass translation units declare file-local types with the same name
			// (e.g. FVisibilitySample), so they can't be merged into unity files.
			BuildConfiguration.bUseUnityBuild = false;

			InitializeDependencyCaches();

			List<UE3ProjectDesc> Projects = new List<UE3ProjectDesc>();
			Projects.Add(new UE3ProjectDesc(LightmassDirectory + "/UnrealLightmass.vcproj"));
			// SwarmInterface is C++/CLI and talks to the managed AgentInterface assembly.
			Projects.Add(new UE3ProjectDesc("UnrealSwarm/SwarmInterfaceMake.vcproj", CPPCLRMode.CLREnabled));
			Utils.CompileProjects(new CPPEnvironment(GlobalCPPEnvironment), FinalLinkEnvironment, Projects);

			SaveDependencyCaches();

			FinalLinkEnvironment.OutputDirectory = Path.GetDirectoryName(OutputPath);
			FinalLinkEnvironment.LocalShadowDirectory = FinalLinkEnvironment.OutputDirectory;
			FinalLinkEnvironment.OutputFilePath = OutputPath;

			List<FileItem> OutputFiles = new List<FileItem>();
			OutputFiles.Add(FinalLinkEnvironment.LinkExecutable());

			GenerateActionCommandPrerequisites();

			return OutputFiles;
		}
	}
}
