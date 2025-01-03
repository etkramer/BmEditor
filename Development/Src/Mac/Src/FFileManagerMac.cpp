/*=============================================================================
 FFileManagerMac.cpp: Unreal Mac based file manager.
 Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "CorePrivate.h"
#include "FFileManagerMac.h"
#include "MacObjCWrapper.h"
#include "UnIpDrv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>


// globals

FString FFileManagerMac::AppDir;
FString FFileManagerMac::DocDir;
FSocket* FFileManagerMac::FileServer;

/*-----------------------------------------------------------------------------
 FArchiveFileReaderMac implementation
 -----------------------------------------------------------------------------*/

FArchiveFileReaderMac::FArchiveFileReaderMac( int InHandle, const ANSICHAR* InFilename, FOutputDevice* InError, INT InSize )
:   Handle          ( InHandle )
,	Filename		( InFilename )
,   Error           ( InError )
,   Size            ( InSize )
,   Pos             ( 0 )
,   BufferBase      ( 0 )
,   BufferCount     ( 0 )
{
	ArIsLoading = ArIsPersistent = 1;
	StatsHandle = FILE_IO_STATS_GET_HANDLE( ANSI_TO_TCHAR(InFilename) );
}

FArchiveFileReaderMac::~FArchiveFileReaderMac()
{
	FILE_IO_STATS_CLOSE_HANDLE( StatsHandle );
	Close();
}

UBOOL FArchiveFileReaderMac::InternalPrecache( INT PrecacheOffset, INT PrecacheSize )
{
	// Only precache at current position and avoid work if precaching same offset twice.
	if( Pos == PrecacheOffset && (!BufferBase || !BufferCount || BufferBase != Pos) )
	{
		BufferBase = Pos;
		BufferCount = Min( Min( PrecacheSize, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), Size-Pos );
		ssize_t Count = 0;
		
		// Read data from device via POSIX read() API.
		{
			SCOPED_FILE_IO_READ_STATS( StatsHandle, BufferCount, Pos );
			Count = ::read( Handle, Buffer, BufferCount );
		}
		
		if( Count!=((ssize_t)BufferCount) )
		{
			ArIsError = 1;
			TCHAR ErrorBuffer[1024];
			Error->Logf( TEXT("ReadFile failed: Count=%i BufferCount=%i Error=%s"), (INT) Count, BufferCount, appGetSystemErrorMessage(ErrorBuffer,1024) );
		}
	}
	return TRUE;
}

void FArchiveFileReaderMac::Seek( INT InPos )
{
	check(InPos>=0);
	check(InPos<=Size);

	if (InPos == Pos || (InPos >= BufferBase && InPos < BufferBase + BufferCount))
	{
		Pos = InPos;
		return;
	}

	if( ::lseek( Handle, InPos, SEEK_SET )==-1 )
	{
		ArIsError = 1;
		TCHAR ErrorBuffer[1024];
		Error->Logf( TEXT("SetFilePointer Failed %i/%i: %i %s"), InPos, Size, Pos, appGetSystemErrorMessage(ErrorBuffer,1024) );
	}
	Pos         = InPos;
	BufferBase  = Pos;
	BufferCount = 0;
}

INT FArchiveFileReaderMac::Tell()
{
	return Pos;
}

INT FArchiveFileReaderMac::TotalSize()
{
	return Size;
}

UBOOL FArchiveFileReaderMac::Close()
{
	if( Handle != -1)
	{
		close( Handle );
	}
	Handle = -1L;
	return !ArIsError;
}

void FArchiveFileReaderMac::Serialize( void* V, INT Length )
{
	while( Length>0 )
	{
		INT Copy = Min( Length, BufferBase+BufferCount-Pos );
		if( Copy==0 )
		{
			if( Length >= ARRAY_COUNT(Buffer) )
			{
				ssize_t Count = 0;
				// Read data from device via POSIX read() API.
				{
					SCOPED_FILE_IO_READ_STATS( StatsHandle, Length, Pos );
					Count = ::read( Handle, V, Length );
				}
				if( Count!=((ssize_t) Length) )
				{
					ArIsError = 1;
					TCHAR ErrorBuffer[1024];
					Error->Logf( TEXT("ReadFile failed: Count=%i Length=%i Error=%s"), (INT) Count, Length, appGetSystemErrorMessage(ErrorBuffer,1024) );
				}
				Pos += Length;
				BufferBase += Length;
				return;
			}
			InternalPrecache( Pos, MAXINT );
			Copy = Min( Length, BufferBase+BufferCount-Pos );
			if( Copy<=0 )
			{
				ArIsError = 1;
				Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, Size );
			}
			if( ArIsError )
			{
				return;
			}
		}
		appMemcpy( V, Buffer+Pos-BufferBase, Copy );
		Pos       += Copy;
		Length    -= Copy;
		V          = (BYTE*)V + Copy;
	}
}



/*-----------------------------------------------------------------------------
 FArchiveFileWriterMac implementation
 -----------------------------------------------------------------------------*/

FArchiveFileWriterMac::FArchiveFileWriterMac( int InHandle, const ANSICHAR* InFilename, FOutputDevice* InError, INT InPos )
:   Handle      ( InHandle )
,	Filename	( InFilename )
,   Error       ( InError )
,   Pos         ( InPos )
,   BufferCount ( 0 )
{
	ArIsSaving = ArIsPersistent = 1;
	StatsHandle = FILE_IO_STATS_GET_HANDLE( ANSI_TO_TCHAR(InFilename) );
}

FArchiveFileWriterMac::~FArchiveFileWriterMac()
{
	FILE_IO_STATS_CLOSE_HANDLE( StatsHandle );
	Close();
}

void FArchiveFileWriterMac::Seek( INT InPos )
{
	Flush();
	if( ::lseek( Handle, InPos, SEEK_SET ) == -1 )
	{
		ArIsError = 1;
		Error->Logf( *LocalizeError("SeekFailed",TEXT("Core")) );
	}
	Pos = InPos;
}

INT FArchiveFileWriterMac::Tell()
{
	return Pos;
}

UBOOL FArchiveFileWriterMac::Close()
{
	Flush();
	if( (Handle != -1) && (close(Handle) == -1) )
	{
		ArIsError = 1;
		Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
	}
	Handle = -1;
	return !ArIsError;
}

void FArchiveFileWriterMac::Serialize( void* V, INT Length )
{
	Pos += Length;
	INT Copy;
	while( Length > (Copy=ARRAY_COUNT(Buffer)-BufferCount) )
	{
		appMemcpy( Buffer+BufferCount, V, Copy );
		BufferCount += Copy;
		Length      -= Copy;
		V            = (BYTE*)V + Copy;
		Flush();
	}
	if( Length )
	{
		appMemcpy( Buffer+BufferCount, V, Length );
		BufferCount += Length;
	}
}

void FArchiveFileWriterMac::Flush()
{
	if( BufferCount )
	{
		SCOPED_FILE_IO_WRITE_STATS( StatsHandle, BufferCount, Pos );
		
		const ssize_t Result = ::write( Handle, Buffer, BufferCount );
		if(Result != BufferCount)
		{
			ArIsError = 1;
			Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
		}
	}
	BufferCount = 0;
}


/*-----------------------------------------------------------------------------
 FFileManagerMac implementation
 -----------------------------------------------------------------------------*/

/**
 * Perform early file manager initialization, particularly finding the application and document directories
 */
void FFileManagerMac::StaticInit()
{
	// set the game name as early as possible, since we need it (engine will call it again, but that's okay)
	extern void appSetGameName();
	appSetGameName();
	
	ANSICHAR dir[MAC_PATH_MAX];
	MacGetDocumentDirectory( dir, MAC_PATH_MAX );
	DocDir = FString( ANSI_TO_TCHAR( dir ) ) + TEXT("/");
	
	MacGetApplicationDirectory( dir, MAC_PATH_MAX );
	// lame hack
	FFilename WrongAppDir(ANSI_TO_TCHAR(dir));
	// strip off the "Applications"
	WrongAppDir = WrongAppDir.GetPath();
	AppDir = WrongAppDir + TEXT("/") + appGetGameName() + TEXT("Game.app/") ;	
}


/**
 * Converts passed in filename to use an absolute path.
 *
 * @param	Filename	filename to convert to use an absolute path, safe to pass in already using absolute path
 * 
 * @return	filename using absolute path
 */
FString FFileManagerMac::ConvertToAbsolutePath( const TCHAR* Filename )
{
	FMacPath PlatformFilename;
	ConvertToMacPath(Filename, PlatformFilename);

	return FString(PlatformFilename);
}

/**
 * Converts a path pointing into the Mac read location, and converts it to the parallel
 * write location. This allows us to read files that the Mac has written out
 *
 * @param AbsolutePath Source path to convert
 *
 * @return Path to the write directory
 */
FString FFileManagerMac::ConvertAbsolutePathToUserPath(const TCHAR* AbsolutePath)
{
	return AbsolutePath;
	FString UserPath( AbsolutePath );
	UserPath = UserPath.Replace( *AppDir, *DocDir, false );
	return UserPath;
}

/*-----------------------------------------------------------------------------
 Public interface
 -----------------------------------------------------------------------------*/

void FFileManagerMac::PreInit()
{
	// set up the read and write root directories

	// always write to documents directory
	appStrcpyANSI(WriteRootPath, TCHAR_TO_UTF8(*DocDir));
	WriteRootPathLen = DocDir.Len();

	// during pre init time, we need to read from the app directory, so the Network file host
	// text file can be read (it will always be part of the application)
	appStrcpyANSI(ReadRootPath, TCHAR_TO_UTF8(*AppDir));
	ReadRootPathLen = AppDir.Len();
}

void FFileManagerMac::Init(UBOOL Startup)
{	
	// read from documents when we aren't GFileManager (because the FFileManagerNetwork
	// is wrapping us and therefore our data files will be in the Documents dir)
	if (GFileManager == this)
	{
		appStrcpyANSI(ReadRootPath, TCHAR_TO_UTF8(*AppDir));
		ReadRootPathLen = AppDir.Len();
	}
	else
	{
		appStrcpyANSI(ReadRootPath, TCHAR_TO_UTF8(*DocDir));
		ReadRootPathLen = DocDir.Len();
	}
}

UBOOL FFileManagerMac::SetDefaultDirectory()
{
	return chdir(TCHAR_TO_UTF8( *AppDir )) != -1;
}

UBOOL FFileManagerMac::SetCurDirectory(const TCHAR* Directory)
{
	return chdir(TCHAR_TO_UTF8( Directory )) != -1;
}

// !!! FIXME: all of this should happen in the base class, since it's all the same on Windows...
// !!! FIXME:  (maybe there should be a FFileManagerPC class above us, so consoles don't get this?)

FString FFileManagerMac::GetCurrentDirectory()
{
	return AppDir;
}

FArchive* FFileManagerMac::CreateFileReader( const TCHAR* InFilename, DWORD Flags, FOutputDevice* Error )
{
	FMacPath PlatformFilename;
	// read save games from the Write directory (passing TRUE as the 3rd param makes it use the Docs directory)
	ConvertToMacPath(InFilename, PlatformFilename, (Flags & FILEREAD_SaveGame) != 0);

	const int Handle = ::open(PlatformFilename, O_RDONLY);
	if( Handle == -1 )
	{
		if( Flags & FILEREAD_NoFail )
		{
			appErrorf( TEXT("Failed to read file: %s"), InFilename );
		}
		return NULL;
	}

	const INT FileSize = GetMacFileSize(Handle);
	if( FileSize < 0)
	{
		::close(Handle);
		if( Flags & FILEREAD_NoFail )
		{
			appErrorf( TEXT("Failed to read file: %s"), InFilename );
		}
		return NULL;
	}

	FArchiveFileReaderMac* Ar = new FArchiveFileReaderMac(Handle,PlatformFilename,Error,FileSize);
	Ar->SetIsSaveGame((Flags & FILEREAD_SaveGame) != 0);
	return Ar;
}

FArchive* FFileManagerMac::CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize )
{
	FMacPath PlatformFilename;
	ConvertToMacPath(Filename, PlatformFilename, TRUE);

	MakeDirectory(*FFilename(Filename).GetPath(), TRUE);

	int UnixFlags = O_WRONLY | O_CREAT;  // open for writing, create if doesn't exist.
	if (Flags & FILEWRITE_NoReplaceExisting)
	{
		UnixFlags |= O_EXCL;
	}

	if (Flags & FILEWRITE_Append)
	{
		UnixFlags |= O_APPEND; // put file pointer at end of existing file.
	}
	else
	{
		UnixFlags |= O_TRUNC;  // truncate existing file to zero bytes.
	}

	//	if (Flags & FILEWRITE_Unbuffered)
	//	{
	//		UnixFlags |= O_SYNC;
	//	}

	int Handle = ::open(PlatformFilename, UnixFlags, 0600);
	if ( (Handle == -1) && (errno == EACCES) && (errno == (Flags & FILEWRITE_EvenIfReadOnly)) )
	{
		struct stat StatBuf;
		if (::stat(PlatformFilename, &StatBuf) != -1)
		{
			StatBuf.st_mode |= S_IWUSR;
			if (::chmod(PlatformFilename, StatBuf.st_mode) != -1)
			{
				Handle = ::open(PlatformFilename, UnixFlags, 0600);
			}
		}
	}

	INT Pos = 0;
	if ( (Handle != -1) && (Flags & FILEWRITE_Append) )
	{
		off_t SeekPos = ::lseek(Handle, 0, SEEK_END);
		if (SeekPos > 0x7FFFFFFF)  // Larger positive value than an INT can hold?
		{
			::close(Handle);
			Handle = -1;  // let this pass through the NoFail check coming up.
		}
		else
		{
			Pos = (INT) SeekPos;
		}
	}

	if (Handle == -1)
	{
		if( Flags & FILEWRITE_NoFail )
		{
			appErrorf( TEXT("Failed to create file: %s"), Filename );
		}
		return NULL;
	}

	FArchiveFileWriterMac* Ar = new FArchiveFileWriterMac(Handle,PlatformFilename,Error,Pos);
	Ar->SetIsSaveGame((Flags & FILEWRITE_SaveGame) != 0);
	return Ar;
}

INT FFileManagerMac::FileSize( const TCHAR* Filename )
{
	FMacPath PlatformFilename;
	ConvertToMacPath(Filename, PlatformFilename);

	struct stat StatBuf;
	if (::stat(PlatformFilename, &StatBuf) == -1)
	{
		return -1;
	}
	if (S_ISREG(StatBuf.st_mode) == 0)  // not a regular file?
	{
		return -1;
	}
	if (StatBuf.st_size > 0x7FFFFFFF)  // bigger than largest positive value that fits in an INT?
	{
		return -1;
	}

	return (INT) StatBuf.st_size;
}

INT FFileManagerMac::UncompressedFileSize( const TCHAR* Filename )
{
//	appErrorf(TEXT("Mac needs a TOC before it will support compressed files"));
	return -1;
}

UBOOL FFileManagerMac::Delete( const TCHAR* Filename, UBOOL RequireExists, UBOOL EvenReadOnly )
{
	// we can only delete from write directory
	FMacPath PlatformFilename;
	ConvertToMacPath(Filename, PlatformFilename, TRUE);

	// You can delete files in Unix if you don't have write permission to them
	//  (the question is whether you have write access to its parent dir), but
	//  in the spirit of what the Windows codebase probably wants, we'll just
	//  ignore EvenReadOnly here.

	INT Result = (::unlink(PlatformFilename) == -1);
	if( !Result )
	{
		if ( (!RequireExists) && ((errno == ENOENT) || (errno == ENOTDIR)) )
		{
			Result = 0;  // we'll call that success.
		}
		else
		{
			debugf( NAME_Error, TEXT("Error deleting file '%s' (errno: %d)"), Filename, errno );
		}
	}
	return Result!=-1;
}

UBOOL FFileManagerMac::IsReadOnly( const TCHAR* Filename )
{
	// we can only delete from write directory
	FMacPath PlatformFilename;
	ConvertToMacPath(Filename, PlatformFilename);

	if (::access(PlatformFilename, F_OK) == -1)
	{
		return FALSE;  // The Windows codepath returns 0 here too...
	}

	if (::access(PlatformFilename, W_OK) == -1)
	{
		if (errno != EACCES)
		{
			debugf( NAME_Error, TEXT("Error reading attributes for '%s'"), Filename );
			return FALSE;  // The Windows codepath returns 0 here too...
		}
		return TRUE;  // it's read only.
	}
	return FALSE;  // it exists and we can write to it.
}

UBOOL FFileManagerMac::Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes )
{
	// move from read
	FMacPath SrcPlatformFilename;
	ConvertToMacPath(Src, SrcPlatformFilename, FALSE);

	// to write dir
	FMacPath DestPlatformFilename;
	ConvertToMacPath(Dest, DestPlatformFilename, TRUE);

	int Result = ::rename(SrcPlatformFilename, DestPlatformFilename);
	if( Result == -1)
	{
		// !!! FIXME: do a copy and delete the original if errno==EXDEV
		debugf( NAME_Error, TEXT("Error moving file '%s' to '%s' (errno: %d)"), Src, Dest, errno );
	}
	return Result!=0;
}

UBOOL FFileManagerMac::MakeDirectory( const TCHAR* Path, UBOOL Tree )
{
	// only create directories in Write directory
	FMacPath PlatformFilename;
	ConvertToMacPath(Path, PlatformFilename, TRUE);

	// let ObjC handle this one
	return MacCreateDirectory(PlatformFilename, Tree ? true : false) == true;
}

UBOOL FFileManagerMac::DeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree )
{
	appErrorf(TEXT("FFileManagerMac:: Probably don't get here"));

	if( Tree )
	{
		return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, TRUE );
	}

	// can only delete from Write directory
	FMacPath PlatformFilename;
	ConvertToMacPath(Path, PlatformFilename, TRUE);

	return ( (rmdir(PlatformFilename) != -1) || ((errno==ENOENT) && (!RequireExists)) );
}


static inline UBOOL WildcardMatch(const TCHAR *Str, const TCHAR *Pattern)
{
	TCHAR SourceChar = *(Str++);
	while (TRUE)
	{
		const TCHAR PatternChar = *(Pattern++);
		if (PatternChar == TEXT('?'))
		{
			if ((SourceChar == TEXT('\0')) || (SourceChar == TEXT('/')))
			{
				return FALSE;
			}
			SourceChar = *(Str++);
		}

		else if (PatternChar == TEXT('*'))
		{
			const TCHAR NextPatternChar = *Pattern;
			if ((NextPatternChar != TEXT('?')) && (NextPatternChar != TEXT('*')))
			{
				while ((SourceChar != TEXT('\0')) && (SourceChar != NextPatternChar))
				{
					SourceChar = *(Str++);
				}
			}
		}

		else
		{
			if (PatternChar != SourceChar)
			{
				return FALSE;
			}
			else if (PatternChar == TEXT('\0'))
			{
				return TRUE;
			}
			SourceChar = *(Str++);
		}
	}

	return TRUE;   // shouldn't hit this.
}

void FFileManagerMac::FindFiles( TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories )
{
	FMacPath PlatformFilename;
	ConvertToMacPath(Filename, PlatformFilename);

	// Separate path and filename.
	FFilename FileParts(PlatformFilename);
	FString FileNameStr = FileParts.GetCleanFilename();
	FString DirStr = FileParts.GetPath();

	// Check for empty path.
	if (DirStr.Len() == 0)
	{
		DirStr = TEXT(".");
	}

	// Open directory, get first entry.
	DIR *Dirp = ::opendir(TCHAR_TO_UTF8(*DirStr));
	if (Dirp != NULL)
	{
		const UBOOL bMatchStarDotStar = (FileNameStr == TEXT("*.*"));
		const UBOOL bMatchWildcard = ( (FileNameStr.InStr(TEXT("*")) != INDEX_NONE) ||
			(FileNameStr.InStr(TEXT("?")) != INDEX_NONE) );
		const UBOOL bMatchAllTypes = (Files && Directories);

		// Check each entry.
		struct dirent *Direntp;
		while( (Direntp = ::readdir(Dirp)) != NULL )
		{
			const FString Entry(UTF8_TO_TCHAR(Direntp->d_name));
			UBOOL bIsMatch = FALSE;

			if ( (Entry == TEXT(".")) || (Entry == TEXT("..")) )
			{
				bIsMatch = FALSE;  // never return current/parent dir entries.
			}
			else if (bMatchStarDotStar)  // match everything, like Windows expects.
			{
				bIsMatch = TRUE;
			}
			else if (bMatchWildcard)  // can't avoid the pattern match?
			{
				bIsMatch = WildcardMatch(*Entry, *FileNameStr);
			}
			else  // fast path if there aren't any wildcard chars...
			{
				bIsMatch = (FileNameStr == Entry);
			}

			if ((bIsMatch) && (!bMatchAllTypes))  // can avoid the stat()?
			{
				bIsMatch = FALSE;  // reset this in case of failure...
				FString FullPath(DirStr);
				FullPath += TEXT("/");
				FullPath += Entry;
				struct stat StatBuf;
				if (::stat(TCHAR_TO_UTF8(*FullPath), &StatBuf) != -1)
				{
					if ( ((Directories) && (S_ISDIR(StatBuf.st_mode))) ||
						((Files) && (S_ISREG(StatBuf.st_mode))) )
					{
						bIsMatch = TRUE;
					}
				}
			}

			if (bIsMatch)
			{
				new(Result)FString(Entry);
			}

		}

		::closedir( Dirp );
	}
}

DOUBLE FFileManagerMac::GetFileAgeSeconds( const TCHAR* Filename )
{
	FMacPath PlatformFilename;
	ConvertToMacPath(Filename, PlatformFilename);

	struct stat FileInfo;
	if (stat(PlatformFilename, &FileInfo) != -1)
	{
		time_t FileTime = FileInfo.st_mtime;
		time_t CurrentTime;
		time( &CurrentTime );
		return difftime( CurrentTime, FileTime );
	}
	return -1.0;
}

DOUBLE FFileManagerMac::GetFileTimestamp( const TCHAR* Filename )
{
	FMacPath PlatformFilename;
	ConvertToMacPath(Filename, PlatformFilename);

	struct stat FileInfo;
	if (stat(PlatformFilename, &FileInfo) != -1)
	{
		return (DOUBLE) FileInfo.st_mtime;
	}
	return -1.0;
}

UBOOL FFileManagerMac::GetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp )
{
	FMacPath PlatformFilename;
	ConvertToMacPath(Filename, PlatformFilename);

	appMemzero( &Timestamp, sizeof(Timestamp) );
	struct stat FileInfo;
	if (stat(PlatformFilename, &FileInfo) != -1)
	{
		time_t FileTime = FileInfo.st_mtime;
		struct tm Time;
		gmtime_r(&FileTime, &Time);
		Timestamp.Day       = Time.tm_mday;
		Timestamp.Month     = Time.tm_mon;
		Timestamp.DayOfWeek = Time.tm_wday;
		Timestamp.Hour      = Time.tm_hour;
		Timestamp.Minute    = Time.tm_min;
		Timestamp.Second    = Time.tm_sec;
		Timestamp.Year      = Time.tm_year + 1900;
		return TRUE;
	}
	return FALSE;
}


/**
 * Updates the modification time of the file on disk to right now, just like the unix touch command
 * @param Filename Path to the file to touch
 * @return TRUE if successful
 */
UBOOL FFileManagerMac::TouchFile(const TCHAR* Filename)
{
	// can only touch in the write dir
	FMacPath PlatformFilename;
	ConvertToMacPath(Filename, PlatformFilename, TRUE);

	// Passing a NULL sets file access/mod time to current system time.
	return (utimes(PlatformFilename, NULL) == 0);
}


/**
 *	Threadsafely converts the platform-independent Unreal filename into platform-specific full path.
 *
 *	@param Filename		Platform-independent Unreal filename
 *	@param Root			Optional root path (don't include trailing path separator)
 *	@return				Platform-dependent full filepath
 **/
FString FFileManagerMac::GetPlatformFilepath( const TCHAR* Filename, const TCHAR* Root )
{
	return ConvertToAbsolutePath(Filename);
}



/*-----------------------------------------------------------------------------
 Internal interface
 -----------------------------------------------------------------------------*/

INT FFileManagerMac::GetMacFileSize(int Handle)
{
	struct stat StatBuf;
	if (::fstat(Handle, &StatBuf) == -1)
	{
		return -1;
	}
	if (S_ISREG(StatBuf.st_mode) == 0)  // not a regular file?
	{
		return -1;
	}
	if (StatBuf.st_size > 0x7FFFFFFF)  // bigger than largest positive value that fits in an INT?
	{
		return -1;
	}
	return (INT) StatBuf.st_size;
}


// See if file specified by (Path) exists with a different case. This
//  assumes that the directories specified exist with the given case, and
//  only looks at the file part of the path. The first file found via
//  readdir() is the one picked, which may not be the first in ASCII order.
//  If a file is found, (Path) is overwritten to reflect the new case, and
//  (true) is returned. If no file is found, (Path) is untouched, and (false)
//  is returned.
//
// Path must be in UTF-8 format! We will be writing to this string, so you
//  must own the buffer.
UBOOL FFileManagerMac::FindAlternateFileCase(char *Path)
{
	// do insensitive check only if the current path doesn't exist...
	if (::access(Path, F_OK) == 0)
	{
		return TRUE;
	}
	
	UBOOL retval = FALSE;
	char *ptr = ::strrchr(Path, TEXT('/'));
	char *filename = (ptr != NULL) ? ptr + 1 : Path;
	char *basedir = (ptr != NULL) ? Path : ((char *) ".");
	
	if (ptr != NULL)
		*ptr = '\0';  // separate dir and filename.
	
	// fast rejection: only check this if there's no wildcard in filename.
	if (::strchr(filename, '*') == NULL)
	{
		DIR *dir = ::opendir(basedir);
		if (dir != NULL)
		{
			struct dirent *ent;
			while (((ent = ::readdir(dir)) != NULL) && (!retval))
			{
				if (appStricmp(UTF8_TO_TCHAR(ent->d_name), UTF8_TO_TCHAR(filename)) == 0)  // a match?
				{
					::strcpy(filename, ent->d_name);  // overwrite with new case.
					retval = TRUE;
				}
			}
			::closedir(dir);
		}
	}
	
	if (ptr != NULL)
		*ptr = '/';  // recombine dir and filename into one path.
	
	return retval;
}


/**
 * Convert a given UE3-style path to one usable on the Mac
 *
 * @param WinPath The source path to convert
 * @param OutPath The resulting Mac usable path
 * @param bIsForWriting TRUE of the path will be written to, FALSE if it will be read from
 */
DOUBLE DEBUG_CaseSensitivityOverhead = 0.0;
void FFileManagerMac::ConvertToMacPath(const TCHAR *WinPath, FMacPath& OutPath, UBOOL bIsForWriting)
{
	// if the path starts with a /, then it was already converted, nothing to do
	if (WinPath[0] == '/')
	{
		// convert to UTF8 and return
		appStrcpyANSI(OutPath, TCHAR_TO_UTF8(WinPath));
		return;
	}

	appStrcpyANSI(OutPath, sizeof(OutPath), TCHAR_TO_UTF8(WinPath));

	// replace slashes
	for (ANSICHAR* Travel = OutPath; *Travel; Travel++)
	{
		if (*Travel == '\\')
		{
			*Travel = '/';
		}
	}
	
	// fast rejection: only check this if the current path doesn't exist...
	if (::access(OutPath, F_OK) != 0)
	{
		char *Cur = OutPath;
		if (*Cur == '/')
		{
			Cur++;
		}
		
		UBOOL KeepLooking = TRUE;
		while (KeepLooking)
		{
			Cur = ::strchr(Cur, TEXT('/'));
			if (Cur != NULL)
			{
				*Cur = '\0';  // null-terminate so we have this chunk of path.
			}
			
			KeepLooking = FindAlternateFileCase(OutPath);
			
			if (Cur == NULL)
			{
				KeepLooking = FALSE;
			}
			else
			{
				*Cur = '/';   // reset string for next check.
				Cur++;
			}
		}
	}
}
