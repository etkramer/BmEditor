@echo off
rem BM: Builds the BmEditor installer. Run after a Release build of BmGame.exe.
setlocal

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
