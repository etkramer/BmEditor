/*=============================================================================
 FFileManagerMac.h: Mac file manager declarations
 Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#ifndef __FFILEMANAGERMAC_H__
#define __FFILEMANAGERMAC_H__

#include "FTableOfContents.h"
#include "FFileManagerGeneric.h"

// make a buffer for the largest file path (1024 was found on the net)
typedef ANSICHAR FMacPath[1024];

/*-----------------------------------------------------------------------------
 FArchiveFileReaderMac
 -----------------------------------------------------------------------------*/

// File manager.
class FArchiveFileReaderMac : public FArchive
	{
	public:
		FArchiveFileReaderMac( int InHandle, const ANSICHAR* InFilename, FOutputDevice* InError, INT InSize );
		~FArchiveFileReaderMac();
		
		virtual void Seek( INT InPos );
		virtual INT Tell();
		virtual INT TotalSize();
		virtual UBOOL Close();
		virtual void Serialize( void* V, INT Length );
		
	protected:
		UBOOL InternalPrecache( INT PrecacheOffset, INT PrecacheSize );
		
		int             Handle;
		/** Handle for stats tracking */
		INT             StatsHandle;
		/** Filename for debugging purposes. */
		FString         Filename;
		FOutputDevice*  Error;
		INT             Size;
		INT             Pos;
		INT             BufferBase;
		INT             BufferCount;
		BYTE            Buffer[4096];
	};


/*-----------------------------------------------------------------------------
 FArchiveFileWriterMac
 -----------------------------------------------------------------------------*/

class FArchiveFileWriterMac : public FArchive
	{
	public:
		FArchiveFileWriterMac( int InHandle, const ANSICHAR* InFilename, FOutputDevice* InError, INT InPos );
		~FArchiveFileWriterMac();
		
		virtual void Seek( INT InPos );
		virtual INT Tell();
		virtual UBOOL Close();
		virtual void Serialize( void* V, INT Length );
		virtual void Flush();
		
	protected:
		int             Handle;
		/** Handle for stats tracking */
		INT             StatsHandle;
		/** Filename for debugging purposes */
		FString         Filename;
		FOutputDevice*  Error;
		INT             Pos;
		INT             BufferCount;
		BYTE            Buffer[4096];
	};


/*-----------------------------------------------------------------------------
 FFileManagerMac
 -----------------------------------------------------------------------------*/

class FFileManagerMac : public FFileManagerGeneric
{
public:
	void PreInit();
	void Init(UBOOL Startup);
	
	/**
	 * Convert a given UE3-style path to one usable on the Mac
	 *
	 * @param WinPath The source path to convert
	 * @param OutPath The resulting Mac usable path
	 * @param bIsForWriting TRUE of the path will be written to, FALSE if it will be read from
	 */
	void ConvertToMacPath(const TCHAR *WinPath, FMacPath& OutPath, UBOOL bIsForWriting=FALSE);
	
	UBOOL SetDefaultDirectory();
	UBOOL SetCurDirectory(const TCHAR* Directory);
	FString GetCurrentDirectory();
	
	FArchive* CreateFileReader( const TCHAR* InFilename, DWORD Flags, FOutputDevice* Error );
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize );
	INT FileSize( const TCHAR* Filename );
	INT	UncompressedFileSize( const TCHAR* Filename );
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 );
	UBOOL IsReadOnly( const TCHAR* Filename );
	UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 );
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 );
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 );
	void FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories );
	DOUBLE GetFileAgeSeconds( const TCHAR* Filename );
	DOUBLE GetFileTimestamp( const TCHAR* Filename );
	UBOOL GetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp );
	
	/**
	 * Updates the modification time of the file on disk to right now, just like the unix touch command
	 * @param Filename Path to the file to touch
	 * @return TRUE if successful
	 */
	UBOOL TouchFile(const TCHAR* Filename);
	
	/**
	 * Converts passed in filename to use an absolute path.
	 *
	 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
	 * 
	 * @return	filename using absolute path
	 */
	virtual FString ConvertToAbsolutePath( const TCHAR* Filename );

	/**
	 * Converts a path pointing into the Mac read location, and converts it to the parallel
	 * write location. This allows us to read files that the Mac has written out
	 *
	 * @param AbsolutePath Source path to convert
	 *
	 * @return Path to the write directory
	 */
	virtual FString ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath);
	
	/**
	 *	Threadsafely converts the platform-independent Unreal filename into platform-specific full path.
	 *
	 *	@param Filename		Platform-independent Unreal filename
	 *	@param Root			Optional root path (don't include trailing path separator)
	 *	@return				Platform-dependent full filepath
	 **/
	virtual FString GetPlatformFilepath( const TCHAR* Filename, const TCHAR* Root=NULL );
		
	/**
	 * Perform early file manager initialization, particularly finding the application and document directories
	 */
	static void StaticInit();
	
	/**
	 * @return the application directory that is filled out in StaticInit()
	 */
	static const FString& GetApplicationDirectory()
	{
		return AppDir;
	}
	
	/**
	 * @return the document directory that is filled out in StaticInit()
	 */
	static const FString& GetDocumentDirectory()
	{
		return DocDir;
	}

protected:
	static INT GetMacFileSize(int Handle);
	static UBOOL FindAlternateFileCase(char *Path);
		
	/** Mac sandboxed paths for the root of the read-only Application bundle directory */
	static FString AppDir;
	/** Mac sandboxed paths for the writable documents directory */
	static FString DocDir;
	
	/** The socket used to talk to the file server to download files */
	static class FSocket* FileServer;

	/** The root path to read files from */
	FMacPath ReadRootPath;
	/** The length of the string in ReadRootPath */
	INT ReadRootPathLen;

	/** The root path to write files to */
	FMacPath WriteRootPath;
	/** The length of the string in WriteRootPath */
	INT WriteRootPathLen;
};

#endif
