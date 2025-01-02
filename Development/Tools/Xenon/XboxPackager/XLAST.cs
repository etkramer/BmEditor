/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Xml;
using System.Xml.Serialization;

namespace XboxPackager
{
	public partial class FolderFile
	{
		[XmlAttributeAttribute()]
		public string clsid = "{EFE8CFD1-82D1-4C08-B633-4C01E701E68F}";

		[XmlAttributeAttribute()]
		public string Name;

		[XmlAttributeAttribute()]
		public string SourcePath;

		public void Init( FileInfo Info )
		{
			Name = Info.Name;
			SourcePath = ".\\" + Name;
		}
	}
	
	public partial class Folder
	{
		[XmlAttributeAttribute()]
		public string clsid = "{6712B0DF-B398-4783-8207-645BDB0EDE73}";

		[XmlElementAttribute( "File" )]
		public List<FolderFile> Files;

		[XmlElementAttribute( "Folder" )]
		public List<Folder> Folders;

		[XmlAttributeAttribute()]
		public string Name;

		public void Init( DirectoryInfo DirInfo )
		{
			Name = DirInfo.Name;
			Files = new List<FolderFile>();
			Folders = new List<Folder>();
		}
	}

	public partial class XboxAutoUpdateProject
	{
		[XmlAttributeAttribute()]
		public string clsid = "{2B29D4F2-D4BA-4E7F-9C44-A852105DB718}";

		public XboxUpdateProjectContents Contents;

		[XmlAttributeAttribute()]
		public string PackageName;

		[XmlAttributeAttribute()]
		public string TitleId;

		[XmlAttributeAttribute()]
		public string BaseVersion;

		[XmlAttributeAttribute()]
		public string OriginalGameBuild;

		[XmlAttributeAttribute()]
		public string UpdatedBinariesPath;

		[XmlAttributeAttribute()]
		public string UpdateVersion;
	}

	public partial class XboxUpdateProjectContents
	{
		[XmlAttributeAttribute()]
		public string clsid = "{8F6862B0-3106-455C-9237-0561E9102657}";

		[XmlElementAttribute( "XexFile" )]
		public XboxContentsXexFile XexFile;

		[XmlElementAttribute( "Folder" )]
		public List<Folder> Folders;
	}

	public partial class XboxContentsXexFile
	{
		[XmlAttributeAttribute]
		public string clsid = "{73C51760-35DD-4174-A8D9-0C419945F8D8}";

		[XmlAttributeAttribute]
		public string Name;

		[XmlAttributeAttribute]
		public string SourcePath;
	}

	public partial class XboxLiveSubmissionProject
	{
		[XmlElementAttribute]
		public XboxAutoUpdateProject AutoUpdateProject;

		[XmlAttributeAttribute]
		public string Version;
	}
}