@echo off
rem BM: Builds the BmEditor installer. Run after a Release build of BmGame.exe.
setlocal

set LOG=%~dp0UnSetup.log

rem BM: Every UnSetup step needs admin, so elevate the whole batch once rather than three times
if "%~1"=="-elevated" goto Elevated

fltmc >nul 2>&1
if not errorlevel 1 goto Build

powershell -NoProfile -Command "$P = Start-Process -FilePath '%~f0' -ArgumentList '-elevated' -Verb RunAs -Wait -PassThru; exit $P.ExitCode"
set RESULT=%errorlevel%
if exist "%LOG%" type "%LOG%"
exit /b %RESULT%

rem The elevated console is not visible, so capture its output for the caller to print
:Elevated
call :Build > "%LOG%" 2>&1
exit /b %errorlevel%

:Build
pushd "%~dp0.."

set UNSETUP=Binaries\UnSetup.exe
set INSTALLER=-installer=BmEditor

echo Creating manifest...
%UNSETUP% %INSTALLER% -createmanifest
if errorlevel 1 goto Failed

echo Building zip...
%UNSETUP% %INSTALLER% -buildinstaller
if errorlevel 1 goto Failed

echo Packaging installer...
%UNSETUP% %INSTALLER% -package
if errorlevel 1 goto Failed

popd
echo Done.
goto :eof

:Failed
popd
echo FAILED.
exit /b 1
