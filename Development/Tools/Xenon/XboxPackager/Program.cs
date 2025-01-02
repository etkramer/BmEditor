/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Reflection;
using System.Text;

namespace XboxPackager
{
	class Program
	{
		static private Assembly CurrentDomain_AssemblyResolve( Object sender, ResolveEventArgs args )
		{
			// Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
			string[] AssemblyInfo = args.Name.Split( ",".ToCharArray() );
			string AssemblyName = Path.GetFullPath( "Binaries\\" + AssemblyInfo[0] + ".dll" );

			if( File.Exists( AssemblyName ) )
			{
				Assembly AssemblyBinary = Assembly.LoadFile( AssemblyName );
				return ( AssemblyBinary );
			}

			return ( null );
		}

		static void Main( string[] Args )
		{
			// Force the CWD to be where the executable is running from
			Environment.CurrentDirectory = Path.Combine( AppDomain.CurrentDomain.BaseDirectory, "..\\.." );

			// Set up and assembly resolver to find common utility assemblies
			AppDomain.CurrentDomain.AssemblyResolve += new ResolveEventHandler( CurrentDomain_AssemblyResolve );

			XLastHandler Handler = new XLastHandler();

			if( !Handler.ParseArguments( Args ) )
			{
				return;
			}

			if( !Handler.PrepareBuild() )
			{
				return;
			}

			if( !Handler.ReadXLast() )
			{
				return;
			}

			if( !Handler.UpdateXLast() )
			{
				return;
			}

			if( !Handler.WriteXLast() )
			{
				return;
			}

			if( !Handler.CleanBuild() )
			{
				return;
			}

			if( !Handler.CreatePackage() )
			{
				return;
			}

			if( !Handler.RearrangeFiles() )
			{
				return;
			}

			Handler.Finalise();
		}
	}
}
