@echo off
rem BM: Builds UnSetupNativeWrapper.exe without needing the VS2008 project or MFC.
setlocal

set VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat
if not exist "%VCVARS%" (
	echo Could not find vcvars32.bat - edit VCVARS in this script.
	exit /b 1
)
call "%VCVARS%" > nul 2>&1

pushd "%~dp0"
set OBJ=%TEMP%\UnSetupNativeWrapper

if not exist "%OBJ%" mkdir "%OBJ%"

rc.exe /nologo /I "." /fo "%OBJ%\Resources.res" "UnSetupNativeWrapper\Resources.rc"
if errorlevel 1 goto Failed

rem Release settings from the original vcproj: static CRT, Unicode, WinMain
cl.exe /nologo /MT /O2 /DWIN32 /DNDEBUG /D_WINDOWS /DUNICODE /D_UNICODE /Fo"%OBJ%\\" /Fe"..\..\..\Binaries\Win32\UnSetupNativeWrapper.exe" NativeWrapper.cpp "%OBJ%\Resources.res" /link /SUBSYSTEM:WINDOWS shlwapi.lib version.lib gdiplus.lib Rpcrt4.lib d3d9.lib user32.lib gdi32.lib advapi32.lib shell32.lib ole32.lib
if errorlevel 1 goto Failed

popd
echo Done.
goto :eof

:Failed
popd
echo FAILED.
exit /b 1
