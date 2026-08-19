/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using Microsoft.Win32;

namespace UnSetup
{
	public partial class Utils
	{
		[StructLayout( LayoutKind.Sequential )]
		public struct OSVERSIONINFO
		{
			public int dwOSVersionInfoSize;
			public int dwMajorVersion;
			public int dwMinorVersion;
			public int dwBuildNumber;
			public int dwPlatformId;
			[MarshalAs( UnmanagedType.ByValTStr, SizeConst = 128 )]
			public string szCSDVersion;
		}

		[DllImport( "kernel32.Dll" )]
		public static extern short GetVersionEx( ref OSVERSIONINFO o );

		public bool WaitForProcess( Process RunningProcess, int Timeout )
		{
			int WaitCount = 0;
			while( !RunningProcess.HasExited )
			{
				Application.DoEvents();

				RunningProcess.WaitForExit( 100 );
				WaitCount++;

				if( WaitCount > Timeout * 10 )
				{
					GenericQuery Query = new GenericQuery( "GQCaptionWaiting", "GQDescWaiting", true, "GQCancel", true, "GQRetry" );
					Query.ShowDialog();
					if( Query.DialogResult == DialogResult.Cancel )
					{
						break;
					}
					else
					{
						WaitCount = 0;
					}
				}
			}

			if( !RunningProcess.HasExited )
			{
				RunningProcess.Kill();
				return ( false );
			}

			if( RunningProcess.ExitCode != 0 )
			{
				return ( false );
			}

			return ( true );
		}

		private int GetOSVersionMajor()
		{
			OSVERSIONINFO OS = new OSVERSIONINFO();

			try
			{
				OS.dwOSVersionInfoSize = Marshal.SizeOf( typeof( OSVERSIONINFO ) );
				GetVersionEx( ref OS );
			}
			catch
			{
			}

			return ( OS.dwMajorVersion );
		}

		// BM
		// Returns null for redists that aren't packaged, so they're skipped rather than failing the install
		private Process StartRedist( string SourceFolder, string Executable, string Arguments )
		{
			if( !File.Exists( Path.Combine( SourceFolder, Executable ) ) )
			{
				return ( null );
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo( Executable, Arguments );
			StartInfo.WorkingDirectory = SourceFolder;
			return ( Process.Start( StartInfo ) );
		}

		public string InstallVCRedist( RedistProgress Progress, string SourceFolder )
		{
			string Status = "OK";
			Process VCRedist = null;

			Progress.LabelDescription.Text = GetPhrase( "RedistVCRedistHeader" );
			if( bStandAloneRedist || Manifest.RootName != "UDK" )
			{
				Progress.LabelDetail.Text = GetPhrase( "RedistVCRedistContentTech" );
			}
			else
			{
				Progress.LabelDetail.Text = GetPhrase( "RedistVCRedistContent" );
			}

			Application.DoEvents();

			// Always install the x86 redists on all OSes
			VCRedist = StartRedist( SourceFolder, "vcredist_x86_vs2008sp1.exe", "/q" );
			if( VCRedist != null && WaitForProcess( VCRedist, 120 ) == false )
			{
				Status = GetPhrase( "RedistVCRedistx86Fail" );
			}

			Application.DoEvents();

			// Install the x64 redist on 64 bit OSes
			if( IntPtr.Size == 8 )
			{
				VCRedist = StartRedist( SourceFolder, "vcredist_x64_vs2008sp1.exe", "/q" );
				if( VCRedist != null && WaitForProcess( VCRedist, 120 ) == false )
				{
					Status = GetPhrase( "RedistVCRedistx64Fail" );
				}
			}

			return ( Status );
		}

		public string InstallDXCutdown( RedistProgress Progress, string SourceFolder )
		{
			string Status = "OK";

			Progress.LabelDescription.Text = GetPhrase( "RedistDXRedistHeader" );
			if( bStandAloneRedist || Manifest.RootName != "UDK" )
			{
				Progress.LabelDetail.Text = GetPhrase( "RedistDXRedistContentTech" );
			}
			else
			{
				Progress.LabelDetail.Text = GetPhrase( "RedistDXRedistContent" );
			}

			Application.DoEvents();

			Process DXRedist = StartRedist( SourceFolder, "DXRedistCutdown\\DXSetup.exe", "/silent" );
			if( DXRedist != null && WaitForProcess( DXRedist, 240 ) == false )
			{
				Status = GetPhrase( "RedistDXRedistFail" );
			}

			return ( Status );
		}

		public string InstallMSCharting( RedistProgress Progress, string SourceFolder )
		{
			string Status = "OK";

			Progress.LabelDescription.Text = GetPhrase( "RedistChartingToolsHeader" );
			if( bStandAloneRedist || Manifest.RootName != "UDK" )
			{
				Progress.LabelDetail.Text = GetPhrase( "RedistChartingToolsContentTech" );
			}
			else
			{
				Progress.LabelDetail.Text = GetPhrase( "RedistChartingToolsContent" );
			}

			Application.DoEvents();

			Process MSChartingRedist = StartRedist( SourceFolder, "MSChart\\SPInstaller.exe", "/q" );
			if( MSChartingRedist != null && WaitForProcess( MSChartingRedist, 120 ) == false )
			{
				Status = GetPhrase( "RedistChartingToolsFail" );
			}

			return ( Status );
		}

		public string InstallAMDCPUDrivers( RedistProgress Progress, string SourceFolder )
		{
			string Status = "OK";

			// Check for Vista/XP
			if( GetOSVersionMajor() < 6 )
			{
				// Check for AMD processor
				string CPUType = "";
				RegistryKey Key = Registry.LocalMachine.OpenSubKey( "HARDWARE\\Description\\System\\CentralProcessor\\0" );
				if( Key != null )
				{
					CPUType = ( string )Key.GetValue( "ProcessorNameString" );
				}

				if( CPUType.ToUpper().StartsWith( "AMD" ) )
				{
					// Install the CPU drivers
					Progress.LabelDescription.Text = GetPhrase( "RedistAMDCPUHeader" );
					if( bStandAloneRedist || Manifest.RootName != "UDK" )
					{
						Progress.LabelDetail.Text = GetPhrase( "RedistAMDCPUContenttTech" );
					}
					else
					{
						Progress.LabelDetail.Text = GetPhrase( "RedistAMDCPUContent" );
					}

					Application.DoEvents();

					Process AMDCPURedist = StartRedist( SourceFolder, "AMD\\amdcpusetup.exe", "/s" );
					if( AMDCPURedist != null && WaitForProcess( AMDCPURedist, 120 ) == false )
					{
						Status = GetPhrase( "RedistAMDCPUFail" );
					}
				}
			}

			return ( Status );
		}
	}
}
